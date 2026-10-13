
#include <gtest/gtest.h>

#include <stdio.h>
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

/* The CLI commands run against the inmemory driver, so a disk written by the
 * writer is read back exactly as plume_cli would read an SD card. */

namespace {

const uint64_t BLOCK_SIZE  = 512;
const uint64_t PAYLOAD     = BLOCK_SIZE - sizeof(struct plume_header);
const uint64_t DISK_BLOCKS = 128;
const uint64_t FAT_SIZE    = 8;

uint8_t stream_byte (uint64_t file, uint64_t i) {
    return (uint8_t) (((i + 7919 * file) * 2654435761u) >> 13);
}

/* Records one file of `size` bytes through a 3 block arena, so the ring
 * wraps many times, and returns the bytes that reached the disk. */
std::string record_file (struct plume_driver* driver, uint64_t file, uint64_t size) {
    std::vector<uint8_t> arena(3 * BLOCK_SIZE);
    struct plume_context ctx;
    ctx.arena_buffer = arena.data();
    ctx.arena_length = arena.size();

    EXPECT_EQ(plume_init(&ctx, driver), PLUME_OK);
    EXPECT_EQ(plume_open_write(&ctx), PLUME_OK);

    std::string written;
    uint64_t chunk_size = 97;
    for (uint64_t offset = 0; offset < size; offset += chunk_size) {
        uint64_t n = std::min(chunk_size, size - offset);
        std::vector<uint8_t> chunk(n);
        for (uint64_t i = 0; i < n; i ++) chunk[i] = stream_byte(file, offset + i);

        uint8_t status;
        while ((status = plume_write(&ctx, chunk.data(), n)) == PLUME_OK_RETRY) {
            EXPECT_TRUE(plume_is_ok(plume_tick(&ctx)));
        }
        EXPECT_EQ(status, PLUME_OK);
        written.append((const char*) chunk.data(), n);
        plume_tick(&ctx);
    }
    for (int i = 0; i < 100; i ++) plume_tick(&ctx);

    /* Only whole blocks are ever flushed. */
    return written.substr(0, (size / PAYLOAD) * PAYLOAD);
}

struct cli_result {
    int         status;
    std::string out;
};

cli_result run (int (*command)(struct plume_driver*, int, char**), struct plume_driver* driver, std::vector<std::string> args) {
    std::vector<char*> argv;
    for (auto& arg : args) argv.push_back(&arg[0]);

    testing::internal::CaptureStdout();
    int status = command(driver, (int) argv.size(), argv.data());
    fflush(stdout);
    return { status, testing::internal::GetCapturedStdout() };
}

struct disk {
    struct plume_driver driver;

    disk () {
        driver = plume_allocate_inmemory_driver(DISK_BLOCKS, BLOCK_SIZE);
        std::vector<uint8_t> arena(BLOCK_SIZE);
        struct plume_context ctx;
        ctx.arena_buffer = arena.data();
        ctx.arena_length = arena.size();
        EXPECT_EQ(plume_clear_disk(&ctx, &driver, FAT_SIZE), PLUME_OK);
    }
    ~disk () {
        plume_free_inmemory_driver(&driver);
    }
    uint8_t* image () {
        return ((struct plume_inmemory_driver*) driver.driver_ptr)->buffer;
    }
};

}

TEST(CliTests, LsOnEmptyDiskSaysNoFiles) {
    disk d;
    cli_result r = run(plume_command_ls, &d.driver, {});
    EXPECT_EQ(r.status, 0);
    EXPECT_NE(r.out.find("No files on disk."), std::string::npos);
}

TEST(CliTests, CatOnEmptyDiskFails) {
    disk d;
    cli_result r = run(plume_command_cat, &d.driver, { "0" });
    EXPECT_NE(r.status, 0);
    EXPECT_EQ(r.out, "");
}

TEST(CliTests, CatReadsBackEveryByteAcrossRingWraps) {
    disk d;
    std::string file0 = record_file(&d.driver, 0, 30 * PAYLOAD + 123);
    std::string file1 = record_file(&d.driver, 1, 12 * PAYLOAD);

    cli_result ls = run(plume_command_ls, &d.driver, {});
    EXPECT_EQ(ls.status, 0);
    EXPECT_NE(ls.out.find("There are 2 files on the disk."), std::string::npos);
    EXPECT_NE(ls.out.find("0       30"), std::string::npos) << ls.out;
    EXPECT_NE(ls.out.find("1       12"), std::string::npos) << ls.out;

    cli_result cat0 = run(plume_command_cat, &d.driver, { "0" });
    EXPECT_EQ(cat0.status, 0);
    EXPECT_EQ(cat0.out.size(), file0.size());
    EXPECT_TRUE(cat0.out == file0);

    cli_result cat1 = run(plume_command_cat, &d.driver, { "1" });
    EXPECT_EQ(cat1.status, 0);
    EXPECT_TRUE(cat1.out == file1);
}

TEST(CliTests, CatOmitsTheUnflushedLastBlock) {
    disk d;
    std::string file0 = record_file(&d.driver, 0, 2 * PAYLOAD + 100);
    EXPECT_EQ(file0.size(), 2 * PAYLOAD);

    cli_result cat0 = run(plume_command_cat, &d.driver, { "0" });
    EXPECT_EQ(cat0.status, 0);
    EXPECT_TRUE(cat0.out == file0);
}

TEST(CliTests, CatOutOfRangeFileIdFails) {
    disk d;
    record_file(&d.driver, 0, 2 * PAYLOAD);

    cli_result r = run(plume_command_cat, &d.driver, { "1" });
    EXPECT_NE(r.status, 0);
    EXPECT_EQ(r.out, "");

    r = run(plume_command_cat, &d.driver, { "-1" });
    EXPECT_NE(r.status, 0);
}

TEST(CliTests, ClearLargerThanDiskIsRejected) {
    disk d;
    std::vector<uint8_t> before(d.image(), d.image() + BLOCK_SIZE);

    cli_result r = run(plume_command_clear, &d.driver, { std::to_string(DISK_BLOCKS + 1) });
    EXPECT_NE(r.status, 0);

    /* The existing settings page is left alone. */
    EXPECT_TRUE(std::equal(before.begin(), before.end(), d.image()));

    r = run(plume_command_clear, &d.driver, { std::to_string(DISK_BLOCKS) });
    EXPECT_EQ(r.status, 0);
}
