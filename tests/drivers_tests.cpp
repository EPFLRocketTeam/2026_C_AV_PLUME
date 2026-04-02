
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/const.h"
    #include "plume/driver.h"
    #include "plume/status.h"
    #include "plume/context.h"
    #include "plume/drivers/inmemory.h"
}

TEST(DriversTests, test_write_block_blocking_works) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    struct plume_context context;
    context.driver = &driver;
    driver.disk_information(&driver, &context.disk_info);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;

    context.fat_size = 64;
    context.next_file_block = 1;
    context.next_valid_block = 64;
    
    uint8_t order[4] = { PLUME_OK_RETRY, PLUME_OK_SENT_DMA, PLUME_OK_RETRY, PLUME_OK };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 4;
    inmem_driver->mock_buffers = order;

    uint8_t buffer[64] = { 1 };
    EXPECT_EQ( plume_write_block_blocking(&context, buffer, 0), PLUME_OK_SENT_DMA );
    EXPECT_EQ( inmem_driver->buffer[0], 1 );

    buffer[0] = 2;
    EXPECT_EQ( plume_write_block_blocking(&context, buffer, 1), PLUME_OK );
    EXPECT_EQ( inmem_driver->buffer[64], 2 );
    
    plume_free_inmemory_driver(&driver);
}
TEST(DriversTests, test_write_block_blocking_fails) {
    struct plume_driver driver = plume_allocate_inmemory_driver (1024, 64);

    struct plume_context context;
    context.driver = &driver;
    driver.disk_information(&driver, &context.disk_info);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;

    context.fat_size = 64;
    context.next_file_block = 1;
    context.next_valid_block = 64;
    
    uint8_t order[4] = { PLUME_OK_RETRY, PLUME_EBAD_DISK, PLUME_OK_RETRY, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = 4;
    inmem_driver->mock_buffers = order;

    uint8_t buffer[64] = { 1 };
    EXPECT_EQ( plume_write_block_blocking(&context, buffer, 0), PLUME_EBAD_DISK );
    EXPECT_EQ( inmem_driver->buffer[0], 0 );

    buffer[0] = 2;
    EXPECT_EQ( plume_write_block_blocking(&context, buffer, 1), PLUME_EBAD_DRIVER );
    EXPECT_EQ( inmem_driver->buffer[64], 0 );
    
    plume_free_inmemory_driver(&driver);
}