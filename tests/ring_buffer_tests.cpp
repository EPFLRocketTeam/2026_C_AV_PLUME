
#include <gtest/gtest.h>

#include <string.h>
#include <vector>

extern "C" {
    #include "plume/drivers/inmemory.h"
    #include "plume/context.h"
    #include "plume/header.h"
    #include "plume/writer.h"
    #include "plume/atomic.h"
    #include "plume/status.h"
    #include "plume/const.h"
}

/* End to end tests of the arena ring buffer and the data area of the disk.
 *
 * Every test writes a deterministic byte stream through plume_write and
 * plume_tick, then reads the inmemory disk image back block by block and
 * checks the page kind, the CRC and every payload byte against the stream. */

namespace {

const uint64_t BLOCK_SIZE = 512;
const uint64_t PAYLOAD    = BLOCK_SIZE - sizeof(struct plume_header);
const uint64_t FAT_SIZE   = 4;
const size_t   GUARD      = 64;
const uint8_t  GUARD_BYTE = 0xA5;

uint8_t stream_byte (uint64_t i) {
    return (uint8_t) ((i * 2654435761u) >> 13);
}

/* Instrumented driver: wraps the inmemory driver, records the highest block
 * written, can fail the next N writes with a given status, and can defer the
 * copy of a write until write_block_ready reports completion, like a DMA that
 * reads the arena after plume_tick returned. */
struct probe {
    uint8_t (*inner_write_block) (void*, struct plume_context*, const uint8_t*, uint64_t);

    uint64_t max_block_end;
    int      write_calls;

    int      fail_next;
    uint8_t  fail_status;

    bool           deferred;
    const uint8_t* pending_buffer;
    uint64_t       pending_block;
    uint32_t       pending_count;
    int            busy_polls;
};
probe g_probe;

uint8_t probe_write_blocks (void* drv, struct plume_context* ctx, const uint8_t* buffer, uint64_t block_id, uint32_t count) {
    g_probe.write_calls ++;
    if (block_id + count > g_probe.max_block_end) {
        g_probe.max_block_end = block_id + count;
    }
    if (g_probe.fail_next > 0) {
        g_probe.fail_next --;
        return g_probe.fail_status;
    }
    if (g_probe.deferred) {
        g_probe.pending_buffer = buffer;
        g_probe.pending_block  = block_id;
        g_probe.pending_count  = count;
        return PLUME_OK_SENT_DMA;
    }
    for (uint32_t i = 0; i < count; i ++) {
        uint8_t status = g_probe.inner_write_block(drv, ctx, buffer + i * ctx->disk_info.block_size, block_id + i);
        if (status != PLUME_OK) {
            return status;
        }
    }
    return PLUME_OK_SENT_DMA;
}
uint8_t probe_write_block (void* drv, struct plume_context* ctx, const uint8_t* buffer, uint64_t block_id) {
    return probe_write_blocks(drv, ctx, buffer, block_id, 1);
}
uint8_t probe_write_block_ready (void* drv, struct plume_context* ctx) {
    if (!g_probe.deferred || g_probe.pending_count == 0) {
        return 1;
    }
    if (g_probe.busy_polls > 0) {
        g_probe.busy_polls --;
        return 0;
    }
    for (uint32_t i = 0; i < g_probe.pending_count; i ++) {
        g_probe.inner_write_block(drv, ctx, g_probe.pending_buffer + i * ctx->disk_info.block_size, g_probe.pending_block + i);
    }
    g_probe.pending_count = 0;
    return 1;
}

struct rig {
    struct plume_driver           driver;
    struct plume_inmemory_driver* mem;
    struct plume_context          ctx;
    std::vector<uint8_t>          arena;
    std::vector<uint8_t>          stream;

    rig (uint64_t disk_blocks, uint64_t arena_length, bool batched) {
        memset(&g_probe, 0, sizeof(g_probe));

        driver = plume_allocate_inmemory_driver(disk_blocks, BLOCK_SIZE);
        mem    = (struct plume_inmemory_driver*) driver.driver_ptr;

        g_probe.inner_write_block = driver.write_block;
        driver.write_block        = probe_write_block;
        driver.write_block_ready  = probe_write_block_ready;
        driver.write_blocks       = batched ? probe_write_blocks : NULL;

        arena.assign(arena_length + GUARD, GUARD_BYTE);
        ctx.arena_buffer = arena.data();
        ctx.arena_length = arena_length;

        EXPECT_EQ(plume_clear_disk(&ctx, &driver, FAT_SIZE), PLUME_OK);
        EXPECT_EQ(plume_open_write(&ctx), PLUME_OK);

        /* Only count what the writer does, not the formatting writes. */
        g_probe.max_block_end = 0;
        g_probe.write_calls   = 0;
    }
    ~rig () {
        plume_free_inmemory_driver(&driver);
    }

    uint8_t write (uint64_t size) {
        std::vector<uint8_t> chunk(size);
        for (uint64_t i = 0; i < size; i ++) chunk[i] = stream_byte(stream.size() + i);

        uint8_t status = plume_write(&ctx, chunk.data(), size);
        if (status == PLUME_OK) {
            stream.insert(stream.end(), chunk.begin(), chunk.end());
        }
        return status;
    }

    void drain () {
        for (int i = 0; i < 10000 && (ctx.rb_number_blocks_used > 0 || ctx.rb_pending_batch_size > 0); i ++) {
            if (plume_tick(&ctx) == PLUME_EDISK_FULL) {
                return;
            }
        }
    }

    bool guard_intact () {
        for (size_t i = ctx.arena_length; i < arena.size(); i ++) {
            if (arena[i] != GUARD_BYTE) return false;
        }
        return true;
    }

    /* Checks every flushed block of the file and returns the number of
     * payload bytes verified. */
    uint64_t verify_disk () {
        uint64_t verified = 0;
        for (uint64_t block = FAT_SIZE; block < ctx.next_valid_block; block ++) {
            uint8_t* page = mem->buffer + block * BLOCK_SIZE;
            EXPECT_EQ(plume_verify_block(page, BLOCK_SIZE, block == FAT_SIZE), PLUME_OK) << "block " << block;

            for (uint64_t j = 0; j < PAYLOAD; j ++) {
                uint64_t index = (block - FAT_SIZE) * PAYLOAD + j;
                if (index >= stream.size()) {
                    ADD_FAILURE() << "block " << block << " holds bytes never written";
                    return verified;
                }
                if (page[sizeof(struct plume_header) + j] != stream[index]) {
                    ADD_FAILURE() << "byte " << index << " differs in block " << block;
                    return verified;
                }
            }
            verified += PAYLOAD;
        }
        return verified;
    }
};

}

TEST(RingBufferTests, ArenaExactlyFullThenOneMoreIsRetried) {
    rig r(64, 4 * BLOCK_SIZE, true);

    EXPECT_EQ(r.ctx.rb_number_bytes_total, 4 * PAYLOAD);
    EXPECT_EQ(r.ctx.rb_max_nb_bytes_per_write, 3 * PAYLOAD);

    EXPECT_EQ(r.write(3 * PAYLOAD), PLUME_OK);
    EXPECT_EQ(r.write(PAYLOAD), PLUME_OK);
    EXPECT_EQ(r.ctx.rb_number_bytes_used, r.ctx.rb_number_bytes_total);
    EXPECT_EQ(r.ctx.rb_number_blocks_used, 4u);
    EXPECT_EQ(r.ctx.rb_rgt_block, r.ctx.rb_lft_block);

    EXPECT_EQ(r.write(1), PLUME_OK_RETRY);
    EXPECT_EQ(r.ctx.rb_number_bytes_used, r.ctx.rb_number_bytes_total);
    EXPECT_EQ(r.ctx.rb_rgt_offset, sizeof(struct plume_header));
    EXPECT_TRUE(r.guard_intact());

    r.drain();
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE + 4);
    EXPECT_EQ(r.verify_disk(), 4 * PAYLOAD);

    /* The arena is usable again after the flush. */
    EXPECT_EQ(r.write(1), PLUME_OK);
}

TEST(RingBufferTests, SingleWriteSpansSeveralBlocks) {
    rig r(64, 8 * BLOCK_SIZE, false);

    EXPECT_EQ(r.write(100), PLUME_OK);
    EXPECT_EQ(r.write(2 * PAYLOAD + 300), PLUME_OK);
    EXPECT_EQ(r.ctx.rb_rgt_block, 2u);
    EXPECT_EQ(r.ctx.rb_rgt_offset, sizeof(struct plume_header) + 400);
    EXPECT_EQ(r.ctx.rb_number_blocks_used, 2u);
    EXPECT_EQ(r.ctx.rb_number_bytes_used, 2 * PAYLOAD + 400);

    r.drain();
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE + 2);
    EXPECT_EQ(r.verify_disk(), 2 * PAYLOAD);
    EXPECT_TRUE(r.guard_intact());
}

static void run_wrap (bool batched, uint64_t arena_length) {
    rig r(256, arena_length, batched);

    const uint64_t sizes[] = { 37, 504, 1, 1000, 255, 1511, 8, 700 };
    uint64_t i = 0;
    while (r.stream.size() < 40 * PAYLOAD) {
        uint64_t size = sizes[i ++ % 8];
        if (size > r.ctx.rb_max_nb_bytes_per_write) size = r.ctx.rb_max_nb_bytes_per_write;
        while (r.write(size) == PLUME_OK_RETRY) {
            ASSERT_TRUE(plume_is_ok(plume_tick(&r.ctx)));
        }
        if (i % 3 == 0) {
            ASSERT_TRUE(plume_is_ok(plume_tick(&r.ctx)));
        }
        ASSERT_TRUE(r.guard_intact());
    }
    r.drain();

    /* Many laps around a ring of a few blocks. */
    EXPECT_GE(r.ctx.next_valid_block - FAT_SIZE, 10 * r.ctx.rb_number_blocks);
    EXPECT_EQ(r.ctx.next_valid_block - FAT_SIZE, r.stream.size() / PAYLOAD);
    EXPECT_EQ(r.verify_disk(), (r.stream.size() / PAYLOAD) * PAYLOAD);
    EXPECT_EQ(r.ctx.rb_number_bytes_used, r.stream.size() % PAYLOAD);
}

TEST(RingBufferTests, WrapSingleBlockDriverReadsBackEveryByte) {
    run_wrap(false, 4 * BLOCK_SIZE);
}
TEST(RingBufferTests, WrapBatchedDriverReadsBackEveryByte) {
    run_wrap(true, 4 * BLOCK_SIZE);
}
TEST(RingBufferTests, WrapArenaNotMultipleOfBlockSize) {
    run_wrap(true, 3 * BLOCK_SIZE + 300);
}

TEST(RingBufferTests, PendingDmaRegionIsNeverOverwritten) {
    rig r(64, 4 * BLOCK_SIZE, true);
    g_probe.deferred   = true;
    g_probe.busy_polls = 50;

    EXPECT_EQ(r.write(3 * PAYLOAD), PLUME_OK);
    EXPECT_EQ(plume_tick(&r.ctx), PLUME_OK_SENT_DMA);
    EXPECT_EQ(r.ctx.rb_pending_batch_size, 3u);

    /* The DMA is still reading the three blocks: only the fourth is free. */
    EXPECT_EQ(r.write(PAYLOAD), PLUME_OK);
    EXPECT_EQ(r.write(1), PLUME_OK_RETRY);
    for (int i = 0; i < 10; i ++) {
        EXPECT_EQ(plume_tick(&r.ctx), PLUME_OK);
        EXPECT_EQ(r.write(1), PLUME_OK_RETRY);
    }

    g_probe.busy_polls = 0;
    r.drain();
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE + 4);
    EXPECT_EQ(r.verify_disk(), 4 * PAYLOAD);
}

static void run_disk_full (bool batched) {
    const uint64_t disk_blocks = 40;
    rig r(disk_blocks, 8 * BLOCK_SIZE, batched);

    uint8_t last = PLUME_OK;
    for (int i = 0; i < 100000 && last != PLUME_EDISK_FULL; i ++) {
        r.write(333);
        last = plume_tick(&r.ctx);
        ASSERT_TRUE(plume_is_ok(last) || last == PLUME_EDISK_FULL) << (int) last;
    }

    EXPECT_EQ(last, PLUME_EDISK_FULL);
    EXPECT_EQ(r.ctx.next_valid_block, disk_blocks);
    EXPECT_EQ(g_probe.max_block_end, disk_blocks);
    EXPECT_EQ(plume_tick(&r.ctx), PLUME_EDISK_FULL);
    EXPECT_EQ(g_probe.max_block_end, disk_blocks);

    /* The arena stays full and refuses writes rather than corrupting. */
    while (r.write(333) == PLUME_OK) {}
    EXPECT_EQ(r.write(333), PLUME_OK_RETRY);
    EXPECT_TRUE(r.guard_intact());

    EXPECT_EQ(r.verify_disk(), (disk_blocks - FAT_SIZE) * PAYLOAD);
}

TEST(RingBufferTests, DiskFullSingleBlockDriverStopsAtLastBlock) {
    run_disk_full(false);
}
TEST(RingBufferTests, DiskFullBatchedDriverStopsAtLastBlock) {
    run_disk_full(true);
}

TEST(RingBufferTests, SaveWriteRollbackTick) {
    rig r(64, 4 * BLOCK_SIZE, true);

    EXPECT_EQ(r.write(200), PLUME_OK);
    struct plume_snapshot snapshot = plume_save(&r.ctx);

    /* A record that does not fit: its first part lands, the second is refused. */
    std::vector<uint8_t> junk(3 * PAYLOAD, 0xEE);
    EXPECT_EQ(plume_write(&r.ctx, junk.data(), PAYLOAD), PLUME_OK);
    EXPECT_EQ(plume_write(&r.ctx, junk.data(), 3 * PAYLOAD), PLUME_OK_RETRY);
    plume_rollback(&r.ctx, &snapshot);

    EXPECT_EQ(r.ctx.rb_number_bytes_used, 200u);
    EXPECT_EQ(plume_tick(&r.ctx), PLUME_OK);
    EXPECT_EQ(r.ctx.rb_pending_batch_size, 0u);

    EXPECT_EQ(r.write(2 * PAYLOAD), PLUME_OK);
    r.drain();
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE + 2);
    EXPECT_EQ(r.verify_disk(), 2 * PAYLOAD);
}

static void run_fail_once (bool batched, uint8_t failure) {
    rig r(64, 8 * BLOCK_SIZE, batched);

    EXPECT_EQ(r.write(3 * PAYLOAD + 10), PLUME_OK);
    g_probe.fail_next   = 1;
    g_probe.fail_status = failure;
    EXPECT_EQ(plume_tick(&r.ctx), failure);
    EXPECT_EQ(r.ctx.rb_pending_batch_size, 0u);
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE);
    EXPECT_EQ(r.ctx.rb_number_blocks_used, 3u);

    r.drain();
    EXPECT_EQ(r.ctx.next_valid_block, FAT_SIZE + 3);
    EXPECT_EQ(r.mem->buffer[FAT_SIZE * BLOCK_SIZE], PLUME_PAGE_FILESTART);
    EXPECT_EQ(r.verify_disk(), 3 * PAYLOAD);
}

TEST(RingBufferTests, BatchedWriteFailsOnceThenRecovers) {
    run_fail_once(true, PLUME_EMOCKED);
}
TEST(RingBufferTests, BatchedWriteRetryOnceThenRecovers) {
    run_fail_once(true, PLUME_OK_RETRY);
}
TEST(RingBufferTests, SingleBlockWriteFailsOnceThenRecovers) {
    run_fail_once(false, PLUME_EMOCKED);
}
