
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/const.h"
    #include "plume/status.h"
    #include "plume/context.h"
    #include "plume/drivers/inmemory.h"
}

TEST(ClearDiskTests, test_plume_clear_disk) {
    struct plume_context context;
    EXPECT_EQ(plume_clear_disk(&context, NULL, 64), PLUME_ENULL);
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);
    EXPECT_EQ(plume_clear_disk(NULL, &driver, 64), PLUME_ENULL);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 63;
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_EARENA_TOO_SMALL);
    context.arena_length = 64;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_OK);

    for (size_t i = 9; i < 65536; i ++) {
        EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[i], 0);
    }

    EXPECT_EQ(context.fat_size, 16);
    EXPECT_EQ(context.next_file_block, 1);
    EXPECT_EQ(context.next_valid_block, 16);

    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[0], PLUME_PAGE_SETTINGS);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[1], 16);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[2], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[3], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[4], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[5], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[6], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[7], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[8], 0);

    plume_free_inmemory_driver(&driver);
}
TEST(ClearDiskTests, test_plume_clear_disk_info_fails) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    uint8_t order[] = { PLUME_EBAD_DISK };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_EBAD_DISK);

    plume_free_inmemory_driver(&driver);
}
TEST(ClearDiskTests, test_plume_clear_disk_page_settings_fail) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    uint8_t order[] = { PLUME_OK, PLUME_EBAD_DISK };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_EBAD_DISK);

    plume_free_inmemory_driver(&driver);
}
TEST(ClearDiskTests, test_plume_clear_disk_page_first_page_fails) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    uint8_t order[] = { PLUME_OK, PLUME_OK, PLUME_EBAD_DISK };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_EBAD_DISK);
    for (size_t i = 9; i < 64; i ++) {
        EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[i], 0);
    }

    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[0], PLUME_PAGE_SETTINGS);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[1], 16);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[2], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[3], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[4], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[5], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[6], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[7], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[8], 0);

    plume_free_inmemory_driver(&driver);
}
TEST(ClearDiskTests, test_plume_clear_disk_page_last_page_fails) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    uint8_t order[1025] = { 0 };
    order[1024] = PLUME_EBAD_DISK;
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_EBAD_DISK);
    for (size_t i = 9; i + 64 < 65536; i ++) {
        EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[i], 0);
    }

    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[0], PLUME_PAGE_SETTINGS);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[1], 16);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[2], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[3], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[4], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[5], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[6], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[7], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[8], 0);

    plume_free_inmemory_driver(&driver);
}
TEST(ClearDiskTests, test_plume_clear_disk_page_fails_after_last_page) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    uint8_t order[1026] = { 0 };
    order[1] = PLUME_OK_SENT_DMA;
    order[1025] = PLUME_EBAD_DISK;
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    for (size_t i = 0; i < 65536; i ++) {
        ((plume_inmemory_driver*) driver.driver_ptr)->buffer[i] = rand() & 0xFF;
    }
    EXPECT_EQ(plume_clear_disk(&context, &driver, 16), PLUME_OK);
    for (size_t i = 9; i < 65536; i ++) {
        EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[i], 0);
    }

    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[0], PLUME_PAGE_SETTINGS);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[1], 16);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[2], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[3], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[4], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[5], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[6], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[7], 0);
    EXPECT_EQ(((plume_inmemory_driver*) driver.driver_ptr)->buffer[8], 0);

    plume_free_inmemory_driver(&driver);
}
