
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/const.h"
    #include "plume/status.h"
    #include "plume/context.h"
    #include "plume/drivers/inmemory.h"
}

TEST(InitTests, test_plume_init_from_null_disk) {
    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);
    EXPECT_EQ(plume_init(&context, NULL), PLUME_ENULL);
    EXPECT_EQ(plume_init(NULL, &driver), PLUME_ENULL);

    // Invalid first page (missing PLUME_PAGE_SETTINGS as first byte)
    EXPECT_EQ(plume_init(&context, &driver), PLUME_EBAD_DISK);
    plume_free_inmemory_driver(&driver);
}
TEST(InitTests, test_plume_init_small_arena) {
    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 63;
    context.arena_buffer = arena_buffer;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);
    
    EXPECT_EQ(plume_init(&context, &driver), PLUME_EARENA_TOO_SMALL);
    plume_free_inmemory_driver(&driver);
}
TEST(InitTests, test_plume_init_disk_info_fails) {
    struct plume_context context;
    plume_driver driver = plume_allocate_inmemory_driver(1024, 64);
    uint8_t order[] = { PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;
    
    EXPECT_EQ(plume_init(&context, &driver), PLUME_EBAD_DRIVER);
    plume_free_inmemory_driver(&driver);
}


TEST(InitTests, test_plume_init_works_on_empty_disk) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 1);
    EXPECT_EQ(new_context.next_valid_block, 64);
    
    plume_free_inmemory_driver(&driver);
}

TEST(InitTests, test_plume_init_works_on_full_fat) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    for (int i = 1; i < 64; i ++) {
        inmem_driver->buffer[i * 64] = PLUME_PAGE_FILEINFO;
    }
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 64);
    EXPECT_EQ(new_context.next_valid_block, 64);
    
    plume_free_inmemory_driver(&driver);
}
TEST(InitTests, test_plume_init_works_on_full_disk) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    for (int i = 64; i < 1024; i ++) {
        inmem_driver->buffer[i * 64] = i == 64 ? PLUME_PAGE_FILESTART : PLUME_PAGE_FILECONT;
    }
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 1);
    EXPECT_EQ(new_context.next_valid_block, 1024);
    
    plume_free_inmemory_driver(&driver);
}
TEST(InitTests, test_plume_init_works_on_partial_disk) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    for (int i = 1; i < 10; i ++) {
        inmem_driver->buffer[i * 64] = PLUME_PAGE_FILEINFO;
    }
    for (int i = 64; i < 641; i ++) {
        inmem_driver->buffer[i * 64] = PLUME_PAGE_FILECONT;
    }
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 10);
    EXPECT_EQ(new_context.next_valid_block, 641);
    
    plume_free_inmemory_driver(&driver);
}



TEST(InitTests, test_plume_init_fails_on_empty_disk_removed_during_page_settings) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);
    
    uint8_t order[] = { PLUME_OK, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_EBAD_DRIVER);

    plume_free_inmemory_driver(&driver);
}

TEST(InitTests, test_plume_init_fails_on_empty_disk_removed_during_fat_search) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);
    
    uint8_t order[] = { PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_EBAD_DRIVER);

    EXPECT_EQ(new_context.fat_size, 64);

    plume_free_inmemory_driver(&driver);
}

TEST(InitTests, test_plume_init_fails_on_empty_disk_removed_during_disk_search) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);
    
    uint8_t order[] = {
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_EBAD_DRIVER);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 1);
    
    plume_free_inmemory_driver(&driver);
}


TEST(InitTests, test_plume_init_works_on_empty_disk_removed_after_init) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);
    
    uint8_t order[] = {
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 1);
    EXPECT_EQ(new_context.next_valid_block, 64);
    
    plume_free_inmemory_driver(&driver);
}
TEST(InitTests, test_plume_init_works_on_empty_ff_disk_removed_after_init) {
    struct plume_driver driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[64];
    struct plume_context context;
    context.arena_length = 64;
    context.arena_buffer = arena_buffer;

    plume_clear_disk(&context, &driver, 64);
    
    uint8_t order[] = {
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK,
        PLUME_OK, PLUME_OK, PLUME_OK, PLUME_OK, PLUME_EBAD_DRIVER };
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    inmem_driver->mock_buffers_size = sizeof(order);
    inmem_driver->mock_buffers = order;

    // Use PLUME_PAGE_EMPTY1 instead of PLUME_PAGE_EMPTY0
    for (int i = 9; i < 65536; i ++) {
        inmem_driver->buffer[i] = PLUME_PAGE_EMPTY1;
    }

    struct plume_context new_context;
    new_context.arena_length = 64;
    new_context.arena_buffer = arena_buffer;
    EXPECT_EQ(plume_init(&new_context, &driver), PLUME_OK);

    EXPECT_EQ(new_context.fat_size, 64);
    EXPECT_EQ(new_context.next_file_block, 1);
    EXPECT_EQ(new_context.next_valid_block, 64);
    
    plume_free_inmemory_driver(&driver);
}
