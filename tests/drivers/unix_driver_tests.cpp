
#include <gtest/gtest.h>

#include <fcntl.h>
#include <malloc.h>
#include <unistd.h>
#include <openssl/evp.h>

extern "C" {
    #include "plume/status.h"
    #include "plume/drivers/unix.h"
}

static std::string sha256_device(const char* device, char hex[65])
{
    int fd = open(device, O_RDONLY);
    if (fd < 0)
        return "";

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);

    uint8_t read_buf[4096];
    ssize_t n;
    while ((n = read(fd, read_buf, sizeof(read_buf))) > 0)
        EVP_DigestUpdate(ctx, read_buf, n);

    close(fd);

    uint8_t  hash[32];
    unsigned hash_len;
    EVP_DigestFinal_ex(ctx, hash, &hash_len);
    EVP_MD_CTX_free(ctx);

    for (int i = 0; i < 32; i++)
        snprintf(hex + i * 2, 3, "%02x", hash[i]);

    return std::string(hex);
}

const char* lorem_ipsum_content = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Fusce mollis elit vitae urna maximus convallis. Integer sed dui a tellus auctor pulvinar. Nunc suscipit gravida elit, non iaculis felis rutrum non. Aliquam et erat egestas, auctor quam ut, vehicula diam. Nullam suscipit ullamcorper nunc eget tristique. Aenean quis magna enim. Curabitur ullamcorper eget diam eu facilisis. Etiam magna quam, rhoncus in porta sed, ultricies ut turpis. Curabitur tincidunt lectus in dapibus maximus."
                                "\n" "Donec faucibus purus sit amet risus fringilla, nec ultricies elit ultricies. Nunc nec leo tellus. Donec congue volutpat justo in sagittis. Mauris tristique urna laoreet, scelerisque ipsum faucibus, sollicitudin turpis. Cras nec fringilla dolor, id porttitor augue. Vestibulum sit amet facilisis purus. Praesent suscipit risus lectus, nec rhoncus libero iaculis ac. Nam consectetur eget mi pulvinar imperdiet."
                                "\n" "Phasellus lobortis orci eget mi fermentum, eu tempor tortor cursus. Aenean malesuada dolor id velit sodales, vel ultrices augue interdum. Nullam sit amet pretium purus, et sollicitudin risus. Vestibulum ante ipsum primis in faucibus orci luctus et ultrices posuere cubilia curae; Nulla efficitur est et purus aliquam, in congue mi volutpat. Sed tempus maximus placerat. Nullam at urna sit amet elit cursus consequat. Donec finibus iaculis ante ut egestas. Quisque tempor lacinia metus dignissim elementum."
                                "\n" "Sed pharetra, mi quis pulvinar malesuada, dui mauris aliquet dolor, eget dignissim turpis libero sit amet felis. Aenean nec velit et felis rutrum tempus. Nullam enim augue, dictum eu accumsan eget, mollis a felis. Phasellus faucibus leo non lacus volutpat maximus. Donec dictum enim vel mollis ultricies. Integer vehicula, nibh et vehicula feugiat, lectus ipsum pulvinar lectus, ut varius purus massa id nisi. Nam ipsum neque, interdum molestie mollis ut, vulputate vel dolor. Vestibulum cursus justo vitae sem imperdiet condimentum. Aenean non sem quam. Sed rhoncus nunc molestie finibus hendrerit. Pellentesque condimentum nec odio at porta an";
const char* lorem_ipsum_sha256 = "e24c5fd16843e25baec9bbbc95a2d29cb723bbfbe002894d48c7f375300c0f41";

TEST(UnixDriverTests, unix_test_allocate_512_16) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("EMPTY_512_16")), PLUME_OK);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 16);

    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver.driver_ptr;
    close(drv->fd);
    drv->fd = -1;

    // This shouldn't fail even if fd = -1
    plume_free_unix_driver(&driver);
}
TEST(UnixDriverTests, unix_test_allocate_1024_8) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("EMPTY_1024_8")), PLUME_OK);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 1024);
    EXPECT_EQ(disk.number_blocks, 8);

    plume_free_unix_driver(&driver);
    // this shouldn't fail even if double free
    plume_free_unix_driver(&driver);
}
TEST(UnixDriverTests, unix_test_allocate_invalid) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, "./unknown"), PLUME_EBAD_UNIX_OPEN);
}
TEST(UnixDriverTests, unix_test_allocate_len_fails) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, "./Makefile"), PLUME_EBAD_UNIX_DISKLEN);
}

TEST(UnixDriverTests, unix_test_lorem_ipsum_read) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM")), PLUME_OK);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    uint8_t buffer[512];
    // Test in-order access
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 0), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i]);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 1), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i + 512]);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 2), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i + 512 * 2]);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 3), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i + 512 * 3]);

    // Test random access needing a change of location
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 1), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i + 512]);

    plume_free_unix_driver(&driver);
}
TEST(UnixDriverTests, unix_test_lorem_ipsum_read_failure_in_order) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM")), PLUME_OK);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    uint8_t buffer[512];
    // Test in-order access
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 0), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i]);
    
    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver.driver_ptr;
    close(drv->fd);
    drv->fd = -1;
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 1), PLUME_EBAD_UNIX_RW);

    plume_free_unix_driver(&driver);
}
TEST(UnixDriverTests, unix_test_lorem_ipsum_read_failure_random_access) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM")), PLUME_OK);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    uint8_t buffer[512];
    // Test in-order access
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 0), PLUME_OK);
    for (int i = 0; i < 512; i ++) EXPECT_EQ(buffer[i], lorem_ipsum_content[i]);
    
    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver.driver_ptr;
    close(drv->fd);
    drv->fd = -1;
    EXPECT_EQ(driver.read_block(driver.driver_ptr, NULL, buffer, 2), PLUME_EBAD_UNIX_LSEEK);

    plume_free_unix_driver(&driver);
}

TEST(UnixDriverTests, unix_test_lorem_ipsum_write_in_order) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM_WO")), PLUME_OK);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content, 0), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 512, 1), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 1024, 2), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 1536, 3), PLUME_OK);

    plume_free_unix_driver(&driver);

    char sha256sum_new_disk[64];
    sha256_device(getenv("LOREM_IPSUM_WO"), sha256sum_new_disk);
    EXPECT_EQ(memcmp(sha256sum_new_disk, lorem_ipsum_sha256, 64), 0);
}
TEST(UnixDriverTests, unix_test_lorem_ipsum_write_random_access) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM_WR")), PLUME_OK);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 1536, 3), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 512, 1), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content, 0), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 1024, 2), PLUME_OK);

    plume_free_unix_driver(&driver);

    char sha256sum_new_disk[65];
    sha256_device(getenv("LOREM_IPSUM_WR"), sha256sum_new_disk);
    EXPECT_EQ(memcmp(sha256sum_new_disk, lorem_ipsum_sha256, 64), 0);
}

TEST(UnixDriverTests, unix_test_lorem_ipsum_write_failure_on_order) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM_WR")), PLUME_OK);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content, 0), PLUME_OK);
    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver.driver_ptr;
    close(drv->fd);
    drv->fd = -1;
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 512, 1), PLUME_EBAD_UNIX_RW);

    plume_free_unix_driver(&driver);
}
TEST(UnixDriverTests, unix_test_lorem_ipsum_write_failure_on_random) {
    struct plume_driver driver;
    EXPECT_EQ(plume_allocate_unix_driver(&driver, getenv("LOREM_IPSUM_WR")), PLUME_OK);

    plume_disk disk;
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk), PLUME_OK);
    EXPECT_EQ(disk.block_size, 512);
    EXPECT_EQ(disk.number_blocks, 4);

    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 1536, 3), PLUME_OK);
    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver.driver_ptr;
    close(drv->fd);
    drv->fd = -1;
    EXPECT_EQ(driver.write_block(driver.driver_ptr, NULL, (const uint8_t*) lorem_ipsum_content + 512, 1), PLUME_EBAD_UNIX_LSEEK);
    
    plume_free_unix_driver(&driver);
}