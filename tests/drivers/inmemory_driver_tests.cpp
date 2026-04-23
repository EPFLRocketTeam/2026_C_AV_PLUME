
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/status.h"
    #include "plume/drivers/inmemory.h"
}

TEST(InMemoryDriverTests, inmemory_test_allocate) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    EXPECT_EQ(inmem_driver->block_count, 1024);
    EXPECT_EQ(inmem_driver->block_size,  64);
    EXPECT_GE(malloc_usable_size(inmem_driver->buffer), 65536);
    for (int i = 0; i < 1024; i ++) {
        EXPECT_EQ(inmem_driver->buffer[i], 0);
    }

    plume_free_inmemory_driver(&driver);
    EXPECT_EQ(driver.driver_ptr, (void*) NULL);
}

TEST(InMemoryDriverTests, inmemory_test_write_null) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    plume_free_inmemory_driver(&driver);

    uint8_t buffer[64];
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_ENULL);
}
TEST(InMemoryDriverTests, inmemory_test_write_oob) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t buffer[64];
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 1023), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 1024), PLUME_EOOB);
    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_write) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t buffer[64];
    for (int i = 0; i < 64; i ++) buffer[i] = i;
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 1024), PLUME_EOOB);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 1023), PLUME_OK);
    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    for (int i = 0; i < 64; i ++) {
        EXPECT_EQ(inmem_driver->buffer[1023 * 64 + i], i);
    }
    for (int i = 0; i < 1023 * 64; i ++) {
        EXPECT_EQ(inmem_driver->buffer[i], 0);
    }
    plume_free_inmemory_driver(&driver);
}

TEST(InMemoryDriverTests, inmemory_test_read_null) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    plume_free_inmemory_driver(&driver);

    uint8_t buffer[64];
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_ENULL);
}
TEST(InMemoryDriverTests, inmemory_test_read_oob) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t buffer[64];
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 1023), PLUME_OK);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 1024), PLUME_EOOB);
    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_read) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    for (int i = 0; i < 64; i ++) {
        inmem_driver->buffer[1023 * 64 + i] = i;
    }

    uint8_t buffer[64] = {0};
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 1024), PLUME_EOOB);
    for (int i = 0; i < 64; i ++) EXPECT_EQ(buffer[i], (uint8_t) 0);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 1023), PLUME_OK);
    for (int i = 0; i < 64; i ++) EXPECT_EQ(buffer[i], (uint8_t) i);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK);
    for (int i = 0; i < 64; i ++) EXPECT_EQ(buffer[i], (uint8_t) 0);
    
    for (int i = 0; i < 64; i ++) {
        EXPECT_EQ(inmem_driver->buffer[1023 * 64 + i], i);
    }
    for (int i = 0; i < 1023 * 64; i ++) {
        EXPECT_EQ(inmem_driver->buffer[i], 0);
    }

    plume_free_inmemory_driver(&driver);
}

TEST(InMemoryDriverTests, inmemory_test_disk_info_null_disk) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    plume_free_inmemory_driver(&driver);

    struct plume_disk disk_info;

    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_ENULL);
}
TEST(InMemoryDriverTests, inmemory_test_disk_info_null_res) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    EXPECT_EQ(driver.disk_information(driver.driver_ptr, nullptr), PLUME_ENULL);
    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_disk_info) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    struct plume_disk disk_info;

    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_OK);
    plume_free_inmemory_driver(&driver);

    EXPECT_EQ(disk_info.number_blocks, 1024);
    EXPECT_EQ(disk_info.block_size, 64);
}
TEST(InMemoryDriverTests, inmemory_ready) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    EXPECT_EQ(inmem_driver->mock_ready, (void*) NULL);
    EXPECT_EQ(inmem_driver->mock_ready_size, 0);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);

    uint8_t order[5] = { 1, 0, 1, 0, 0 };
    inmem_driver->mock_ready = order;
    inmem_driver->mock_ready_size = 5;
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 0);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 0);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 0);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);
    EXPECT_EQ(driver.write_block_ready(driver.driver_ptr, NULL), 1);
    plume_free_inmemory_driver(&driver);
}


TEST(InMemoryDriverTests, inmemory_test_reads_setbehaviors) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t order[5] = { 0, PLUME_EMOCKED, PLUME_EMOCKED, 0, PLUME_EMOCKED };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 5;
    inmem_driver->mock_buffers = order;

    uint8_t buffer[64];
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK);
    EXPECT_EQ(driver.read_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);

    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_write_setbehaviors) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t order[5] = { 0, PLUME_EMOCKED, PLUME_EMOCKED, 0, PLUME_EMOCKED };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 5;
    inmem_driver->mock_buffers = order;

    uint8_t buffer[64];
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);

    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_write_setbehaviors_sent_dma) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    uint8_t order[5] = { 1, PLUME_EMOCKED, PLUME_EMOCKED, 1, PLUME_EMOCKED };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 5;
    inmem_driver->mock_buffers = order;

    uint8_t buffer[64];
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK_SENT_DMA);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_OK_SENT_DMA);
    EXPECT_EQ(driver.write_block(driver.driver_ptr, nullptr, buffer, 0), PLUME_EMOCKED);

    plume_free_inmemory_driver(&driver);
}
TEST(InMemoryDriverTests, inmemory_test_disk_info_setbehaviors) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);
    uint8_t order[5] = { 0, PLUME_EMOCKED, PLUME_EMOCKED, 0, PLUME_EMOCKED };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 5;
    inmem_driver->mock_buffers = order;

    struct plume_disk disk_info;

    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_OK);
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_EMOCKED);
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_EMOCKED);
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_OK);
    EXPECT_EQ(driver.disk_information(driver.driver_ptr, &disk_info), PLUME_EMOCKED);

    plume_free_inmemory_driver(&driver);
}
