
#include <gtest/gtest.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

extern "C" {
    #include "plume/drivers/inmemory.h"
    #include "plume/cli/command.h"
    #include "plume/context.h"
    #include "plume/header.h"
    #include "plume/writer.h"
    #include "plume/status.h"
    #include "plume/const.h"
}

/* Circular disk mode, end to end on the inmemory driver.
 *
 * Layout used by every test: 40 blocks of 512 bytes, FAT of 4 blocks, so the
 * data area is blocks 4 to 39 (36 blocks). A "virtual" block v counts blocks
 * written since the disk was cleared: v = (lap - 1) * 36 + block. */

namespace {

const uint64_t BLOCK_SIZE = 512;
const uint64_t PAYLOAD    = BLOCK_SIZE - sizeof(struct plume_header);
const uint64_t DISK       = 40;
const uint64_t FAT        = 4;
const uint64_t DATA       = DISK - FAT;

uint8_t stream_byte (uint64_t file, uint64_t i) {
    return (uint8_t) (((i + 7919 * file) * 2654435761u) >> 13);
}

uint8_t (*g_inner_write_block) (void*, struct plume_context*, const uint8_t*, uint64_t);
uint64_t g_max_block_end;

uint8_t probe_write_block (void* drv, struct plume_context* ctx, const uint8_t* buffer, uint64_t block_id) {
    if (block_id + 1 > g_max_block_end) g_max_block_end = block_id + 1;
    return g_inner_write_block(drv, ctx, buffer, block_id);
}
uint8_t probe_write_blocks (void* drv, struct plume_context* ctx, const uint8_t* buffer, uint64_t block_id, uint32_t count) {
    if (block_id + count > g_max_block_end) g_max_block_end = block_id + count;
    for (uint32_t i = 0; i < count; i ++) {
        uint8_t status = g_inner_write_block(drv, ctx, buffer + i * BLOCK_SIZE, block_id + i);
        if (status != PLUME_OK) return status;
    }
    return PLUME_OK_SENT_DMA;
}

struct cli_result {
    int         status;
    std::string out;
};

struct disk {
    struct plume_driver  driver;
    std::vector<uint8_t> arena;

    disk (bool circular, bool batched = false) : arena(4 * BLOCK_SIZE) {
        driver = plume_allocate_inmemory_driver(DISK, BLOCK_SIZE);
        g_inner_write_block = driver.write_block;
        driver.write_block  = probe_write_block;
        driver.write_blocks = batched ? probe_write_blocks : NULL;

        struct plume_context ctx;
        ctx.arena_buffer = arena.data();
        ctx.arena_length = arena.size();
        if (circular) {
            EXPECT_EQ(plume_clear_disk_circular(&ctx, &driver, FAT), PLUME_OK);
        } else {
            EXPECT_EQ(plume_clear_disk(&ctx, &driver, FAT), PLUME_OK);
        }
        g_max_block_end = 0;
    }
    ~disk () {
        plume_free_inmemory_driver(&driver);
    }

    uint8_t* block (uint64_t id) {
        return ((struct plume_inmemory_driver*) driver.driver_ptr)->buffer + id * BLOCK_SIZE;
    }
    uint32_t lap (uint64_t id) {
        uint8_t* page = block(id);
        return page[1] | (page[2] << 8) | (page[3] << 16);
    }

    /* Opens a new file and writes `size` bytes of its stream through a 3 block
     * arena. Returns the bytes that reached the disk (whole blocks only). */
    std::string record (uint64_t file, uint64_t size, struct plume_context* out = NULL) {
        struct plume_context local;
        struct plume_context* ctx = out ? out : &local;
        ctx->arena_buffer = arena.data();
        ctx->arena_length = 3 * BLOCK_SIZE;

        EXPECT_EQ(plume_init(ctx, &driver), PLUME_OK);
        EXPECT_EQ(plume_open_write(ctx), PLUME_OK);

        std::string written;
        for (uint64_t offset = 0; offset < size; offset += 97) {
            uint64_t n = std::min<uint64_t>(97, size - offset);
            std::vector<uint8_t> chunk(n);
            for (uint64_t i = 0; i < n; i ++) chunk[i] = stream_byte(file, offset + i);

            uint8_t status;
            while ((status = plume_write(ctx, chunk.data(), n)) == PLUME_OK_RETRY) {
                EXPECT_TRUE(plume_is_ok(plume_tick(ctx)));
            }
            EXPECT_EQ(status, PLUME_OK);
            written.append((const char*) chunk.data(), n);
            EXPECT_TRUE(plume_is_ok(plume_tick(ctx)));
        }
        for (int i = 0; i < 100; i ++) EXPECT_TRUE(plume_is_ok(plume_tick(ctx)));
        EXPECT_EQ(ctx->rb_number_blocks_used, 0u);
        return written.substr(0, (size / PAYLOAD) * PAYLOAD);
    }

    /* Checks that virtual block v of a file whose first virtual block is
     * `start` holds payload block `index` of that file's stream. */
    void expect_virtual (uint64_t v, uint64_t start, uint64_t file, uint64_t index) {
        uint64_t id  = FAT + (v - FAT) % DATA;
        uint32_t lap = (uint32_t) ((v - FAT) / DATA + 1);
        uint8_t* page = block(id);
        EXPECT_EQ(lap, this->lap(id)) << "block " << id;
        EXPECT_EQ(plume_verify_block(page, BLOCK_SIZE, v == start), PLUME_OK) << "block " << id;
        for (uint64_t j = 0; j < PAYLOAD; j ++) {
            if (page[sizeof(struct plume_header) + j] != stream_byte(file, index * PAYLOAD + j)) {
                ADD_FAILURE() << "block " << id << " byte " << j;
                return;
            }
        }
    }

    cli_result run (int (*command)(struct plume_driver*, int, char**), std::vector<std::string> args) {
        std::vector<char*> argv;
        for (auto& arg : args) argv.push_back(&arg[0]);

        testing::internal::CaptureStdout();
        int status = command(&driver, (int) argv.size(), argv.data());
        fflush(stdout);
        return { status, testing::internal::GetCapturedStdout() };
    }
};

}

TEST(CircularTests, ClearWritesTheModeInTheSettingsPage) {
    {
        disk d(false);
        EXPECT_EQ(d.block(0)[0], PLUME_PAGE_SETTINGS);
        EXPECT_EQ(d.block(0)[1], FAT);
        EXPECT_EQ(d.block(0)[9], 0);
    }
    {
        disk d(true);
        EXPECT_EQ(d.block(0)[0], PLUME_PAGE_SETTINGS);
        EXPECT_EQ(d.block(0)[1], FAT);
        EXPECT_EQ(d.block(0)[9], 1);
        for (uint64_t i = 10; i < BLOCK_SIZE; i ++) EXPECT_EQ(d.block(0)[i], 0);

        struct plume_context ctx;
        ctx.arena_buffer = d.arena.data();
        ctx.arena_length = d.arena.size();
        EXPECT_EQ(plume_init(&ctx, &d.driver), PLUME_OK);
        EXPECT_EQ(ctx.circular, 1);
        EXPECT_EQ(ctx.lap, 1u);
        EXPECT_EQ(ctx.next_valid_block, FAT);

        // A circular disk needs at least one data block.
        EXPECT_EQ(plume_clear_disk_circular(&ctx, &d.driver, DISK), PLUME_EOOB);
        EXPECT_EQ(plume_clear_disk_circular(&ctx, &d.driver, 0), PLUME_EOOB);
        EXPECT_EQ(d.block(0)[9], 1);
    }
}

TEST(CircularTests, FlagOffKeepsDiskFull) {
    disk d(false, true);
    struct plume_context ctx;
    ctx.arena_buffer = d.arena.data();
    ctx.arena_length = 3 * BLOCK_SIZE;
    ASSERT_EQ(plume_init(&ctx, &d.driver), PLUME_OK);
    EXPECT_EQ(ctx.circular, 0);
    ASSERT_EQ(plume_open_write(&ctx), PLUME_OK);

    std::vector<uint8_t> chunk(333, 0x42);
    uint8_t last = PLUME_OK;
    for (int i = 0; i < 10000 && last != PLUME_EDISK_FULL; i ++) {
        plume_write(&ctx, chunk.data(), chunk.size());
        last = plume_tick(&ctx);
    }
    EXPECT_EQ(last, PLUME_EDISK_FULL);
    EXPECT_EQ(ctx.next_valid_block, DISK);
    EXPECT_EQ(g_max_block_end, DISK);
    EXPECT_EQ(plume_tick(&ctx), PLUME_EDISK_FULL);
    EXPECT_EQ(g_max_block_end, DISK);
    // The arena fills up and then refuses writes.
    while (plume_write(&ctx, chunk.data(), chunk.size()) == PLUME_OK) {}
    EXPECT_EQ(plume_write(&ctx, chunk.data(), chunk.size()), PLUME_OK_RETRY);

    // A fresh init still sees a full disk.
    struct plume_context again;
    again.arena_buffer = d.arena.data();
    again.arena_length = 3 * BLOCK_SIZE;
    EXPECT_EQ(plume_init(&again, &d.driver), PLUME_OK);
    EXPECT_EQ(again.next_valid_block, DISK);
}

static void run_wrap_once (bool batched) {
    disk d(true, batched);
    struct plume_context ctx;
    d.record(0, 50 * PAYLOAD + 10, &ctx);

    EXPECT_LE(g_max_block_end, DISK);
    EXPECT_EQ(ctx.lap, 2u);
    EXPECT_EQ(ctx.next_valid_block, FAT + 14);

    // Second lap: blocks 4 to 17 hold file blocks 36 to 49.
    for (uint64_t v = FAT + DATA; v < FAT + 50; v ++) d.expect_virtual(v, FAT, 0, v - FAT);
    // What survives of the first lap: blocks 18 to 39, file blocks 14 to 35.
    for (uint64_t v = FAT + 14; v < FAT + DATA; v ++) d.expect_virtual(v, FAT, 0, v - FAT);
}

TEST(CircularTests, WrapOnceReadsBackTheSecondLap) {
    run_wrap_once(false);
}
TEST(CircularTests, WrapOnceBatchedNeverWritesPastTheEnd) {
    run_wrap_once(true);
}

TEST(CircularTests, InitOnAnEmptyCircularDisk) {
    disk d(true);
    struct plume_context ctx;
    d.record(0, 10 * PAYLOAD, &ctx);

    struct plume_context again;
    again.arena_buffer = d.arena.data();
    again.arena_length = 3 * BLOCK_SIZE;
    EXPECT_EQ(plume_init(&again, &d.driver), PLUME_OK);
    EXPECT_EQ(again.next_valid_block, FAT + 10);
    EXPECT_EQ(again.lap, 1u);
}

TEST(CircularTests, InitOnAWrappedDiskFindsTheHead) {
    disk d(true);
    d.record(0, 50 * PAYLOAD + 10);

    struct plume_context ctx;
    std::string file1 = d.record(1, 2 * PAYLOAD, &ctx);

    // The head was found at block 18 (lap 2): file 1 starts there.
    uint8_t* info = d.block(2);
    EXPECT_EQ(info[0], PLUME_PAGE_FILEINFO);
    uint64_t start;
    memcpy(&start, info + 1, sizeof(start));
    EXPECT_EQ(start, FAT + 14);
    EXPECT_EQ(info[9] | (info[10] << 8) | (info[11] << 16), 2);

    EXPECT_EQ(d.block(FAT + 14)[0], PLUME_PAGE_FILESTART);
    d.expect_virtual(FAT + 50, FAT + 50, 1, 0);
    d.expect_virtual(FAT + 51, FAT + 50, 1, 1);
    EXPECT_EQ(ctx.next_valid_block, FAT + 16);

    // Block 20 still holds lap 1 data of file 0.
    EXPECT_EQ(d.lap(FAT + 16), 1u);
}

TEST(CircularTests, InitWithTheHeadExactlyAtTheDiskEnd) {
    disk d(true);
    struct plume_context ctx;
    d.record(0, DATA * PAYLOAD, &ctx);
    EXPECT_EQ(ctx.next_valid_block, FAT);
    EXPECT_EQ(ctx.lap, 2u);

    struct plume_context again;
    again.arena_buffer = d.arena.data();
    again.arena_length = 3 * BLOCK_SIZE;
    EXPECT_EQ(plume_init(&again, &d.driver), PLUME_OK);
    EXPECT_EQ(again.next_valid_block, FAT);
    EXPECT_EQ(again.lap, 2u);

    d.record(1, PAYLOAD);
    d.expect_virtual(FAT + DATA, FAT + DATA, 1, 0);
    EXPECT_EQ(d.lap(FAT + 1), 1u);
}

TEST(CircularTests, LsAndCatOnAWrappedDisk) {
    disk d(true);
    std::string file0 = d.record(0, 20 * PAYLOAD);
    std::string file1 = d.record(1, 30 * PAYLOAD);

    // Virtual blocks: file 0 is 4 to 23, file 1 is 24 to 53, the oldest
    // block still on disk is 18, so file 0 keeps its last 6 blocks.
    cli_result ls = d.run(plume_command_ls, {});
    EXPECT_EQ(ls.status, 0);
    EXPECT_NE(ls.out.find("0       6 (first 14 blocks overwritten)\n"), std::string::npos) << ls.out;
    EXPECT_NE(ls.out.find("1       30\n"), std::string::npos) << ls.out;

    cli_result cat1 = d.run(plume_command_cat, { "1" });
    EXPECT_EQ(cat1.status, 0);
    EXPECT_TRUE(cat1.out == file1);

    cli_result cat0 = d.run(plume_command_cat, { "0" });
    EXPECT_NE(cat0.status, 0);
    EXPECT_TRUE(cat0.out == file0.substr(14 * PAYLOAD));
}

TEST(CircularTests, CatOfAFileThatOverwroteItsOwnStart) {
    disk d(true);
    std::string file0 = d.record(0, 50 * PAYLOAD + 300);

    cli_result ls = d.run(plume_command_ls, {});
    EXPECT_NE(ls.out.find("0       36 (first 14 blocks overwritten)\n"), std::string::npos) << ls.out;

    cli_result cat0 = d.run(plume_command_cat, { "0" });
    EXPECT_NE(cat0.status, 0);
    EXPECT_EQ(cat0.out.size(), DATA * PAYLOAD);
    EXPECT_TRUE(cat0.out == file0.substr(14 * PAYLOAD));
}

TEST(CircularTests, CatOfAFileEntirelyOverwritten) {
    disk d(true);
    d.record(0, 10 * PAYLOAD);
    std::string file1 = d.record(1, 40 * PAYLOAD);

    cli_result ls = d.run(plume_command_ls, {});
    EXPECT_NE(ls.out.find("0       0 (first 10 blocks overwritten)\n"), std::string::npos) << ls.out;
    EXPECT_NE(ls.out.find("1       36 (first 4 blocks overwritten)\n"), std::string::npos) << ls.out;

    cli_result cat0 = d.run(plume_command_cat, { "0" });
    EXPECT_NE(cat0.status, 0);
    EXPECT_EQ(cat0.out, "");

    cli_result cat1 = d.run(plume_command_cat, { "1" });
    EXPECT_NE(cat1.status, 0);
    EXPECT_TRUE(cat1.out == file1.substr(4 * PAYLOAD));
}

TEST(CircularTests, CliClearCircular) {
    disk d(false);
    cli_result r = d.run(plume_command_clear, { "8", "circular" });
    EXPECT_EQ(r.status, 0);
    EXPECT_EQ(d.block(0)[9], 1);
    EXPECT_EQ(d.block(0)[1], 8);

    r = d.run(plume_command_clear, { "8", "round" });
    EXPECT_NE(r.status, 0);
    EXPECT_EQ(d.block(0)[9], 1);

    r = d.run(plume_command_clear, { "8" });
    EXPECT_EQ(r.status, 0);
    EXPECT_EQ(d.block(0)[9], 0);
}
