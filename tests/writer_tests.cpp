
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/drivers/inmemory.h"
    #include "plume/context.h"
    #include "plume/header.h"
    #include "plume/writer.h"
    #include "plume/status.h"
    #include "plume/const.h"
}
TEST(WriterTests, TestOpenFileForNull) {
    struct plume_context context;
    context.driver = NULL;
    EXPECT_EQ(plume_open_write(NULL), PLUME_ENULL);
    EXPECT_EQ(plume_open_write(&context), PLUME_ENULL);
}
TEST(WriterTests, TestSimpleOpenFile) {
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    
    plume_clear_disk(&context, &driver, 3);
    EXPECT_EQ(context.next_file_block,  1);
    EXPECT_EQ(context.next_valid_block, 3);
    EXPECT_EQ(plume_open_write(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[64], PLUME_PAGE_FILEINFO);
    EXPECT_EQ(*((uint64_t*) (inmem_driver->buffer + 65)), 3);
    context.next_valid_block = 21;
    EXPECT_EQ(plume_open_write(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[128], PLUME_PAGE_FILEINFO);
    EXPECT_EQ(*((uint64_t*) (inmem_driver->buffer + 129)), 21);
    EXPECT_EQ(plume_open_write(&context), PLUME_EFAT_FULL);

    plume_free_inmemory_driver(&driver);
}
TEST(WriterTests, SecondOpenFailsOnOrder) {
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(1024, 64);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    
    plume_clear_disk(&context, &driver, 3);
    EXPECT_EQ(context.next_file_block,  1);
    EXPECT_EQ(context.next_valid_block, 3);
    EXPECT_EQ(plume_open_write(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[64], PLUME_PAGE_FILEINFO);
    EXPECT_EQ(*((uint64_t*) (inmem_driver->buffer + 65)), 3);
    context.next_valid_block = 21;
    uint8_t order[1] = { PLUME_EMOCKED };
    inmem_driver->mock_buffers = order;
    inmem_driver->mock_buffers_size = 1;
    EXPECT_EQ(plume_open_write(&context), PLUME_EMOCKED);
    EXPECT_EQ(inmem_driver->buffer[128], 0);
    EXPECT_EQ(*((uint64_t*) (inmem_driver->buffer + 129)), 0);

    plume_free_inmemory_driver(&driver);
};

TEST(WriterTests, TestWriteSimpleHelloWorld) {
    const int block_size = 7 + sizeof(plume_header);
    
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(1024, block_size);

    context.driver = NULL;
    EXPECT_EQ(plume_write(NULL, (const uint8_t*) "hello, ", 7), PLUME_ENULL);
    EXPECT_EQ(plume_write(&context, (const uint8_t*) "hello, ", 7), PLUME_ENULL);
    EXPECT_EQ(plume_tick(NULL), PLUME_ENULL);
    EXPECT_EQ(plume_tick(&context), PLUME_ENULL);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    
    plume_clear_disk(&context, &driver, 3);
    plume_open_write(&context);

    EXPECT_EQ(plume_write(&context, (const uint8_t*) "hello, world !", 14), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[3 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(plume_tick (&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[3 * block_size], PLUME_PAGE_FILESTART);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 8],  'h');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 9],  'e');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 10], 'l');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 11], 'l');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 12], 'o');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 13], ',');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 14], ' ');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 8],  0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 9],  0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 10], 0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 11], 0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 12], 0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 13], 0);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 14], 0);
    EXPECT_EQ(plume_tick (&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[block_size], PLUME_PAGE_FILEINFO);
    EXPECT_EQ(inmem_driver->buffer[3 * block_size], PLUME_PAGE_FILESTART);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size], PLUME_PAGE_FILECONT);
    EXPECT_EQ(*((uint32_t*) (inmem_driver->buffer + 3 * block_size + 4)), 0x11ea5699);
    EXPECT_EQ(*((uint32_t*) (inmem_driver->buffer + 4 * block_size + 4)), 0x29d02040);

    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 8],  'h');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 9],  'e');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 10], 'l');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 11], 'l');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 12], 'o');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 13], ',');
    EXPECT_EQ(inmem_driver->buffer[3 * block_size + 14], ' ');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 8],  'w');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 9],  'o');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 10], 'r');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 11], 'l');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 12], 'd');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 13], ' ');
    EXPECT_EQ(inmem_driver->buffer[4 * block_size + 14], '!');

    plume_free_inmemory_driver(&driver);
}

TEST(WriterTests, TestPlumeRetryAndArenaTooSmall) {
    const int block_size = 7 + sizeof(plume_header);
    
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(1024, block_size);

    context.driver = NULL;
    EXPECT_EQ(plume_write(NULL, (const uint8_t*) "hello, ", 7), PLUME_ENULL);
    EXPECT_EQ(plume_write(&context, (const uint8_t*) "hello, ", 7), PLUME_ENULL);
    EXPECT_EQ(plume_tick(NULL), PLUME_ENULL);
    EXPECT_EQ(plume_tick(&context), PLUME_ENULL);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 64;
    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;
    
    plume_clear_disk(&context, &driver, 3);
    plume_open_write(&context);

    // number blocks in arena
    const int nb_blocks    = 64 / block_size;
    const int max_nb_bytes = nb_blocks * (block_size - sizeof(struct plume_header));
    EXPECT_EQ(context.rb_number_bytes_total, max_nb_bytes);

    uint8_t buffer[max_nb_bytes + 20];
    for (int i = 0; i < max_nb_bytes; i ++) buffer[i] = i;
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header)) + 1), PLUME_EARENA_TOO_SMALL);
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header))), PLUME_OK);
    for (int i = 0; i < 20; i ++) plume_tick(&context);
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes / 2), PLUME_OK);
    EXPECT_EQ(context.rb_number_bytes_used, 14);
    for (int i = 0; i < 20; i ++) plume_tick(&context);
    EXPECT_EQ(context.rb_number_bytes_used, 0);
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes / 2), PLUME_OK);
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes / 2), PLUME_OK);
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header))), PLUME_OK_RETRY);
    for (int i = 0; i < 20; i ++) plume_tick(&context);

    const int total_written = 9;
    uint8_t bases [12] = {
        0, 0, 0,
        0, 7, 14,
        0, 7,
        0, 7,
        0, 7
    };
    for (int i = 3; i < 3 + total_written; i ++) {
        int base = bases[i];
        for (int j = 8; j < block_size; j ++) {
            EXPECT_EQ(inmem_driver->buffer[i * block_size + j], base + j - 8);
        }
    }

    plume_free_inmemory_driver(&driver);
}

TEST(WriterTests, TestPlumeTickDiskFull) {
    const int block_size = 7 + sizeof(plume_header);
    
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(6, block_size);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 79;
    
    plume_clear_disk(&context, &driver, 3);
    plume_open_write(&context);

    // number blocks in arena
    const int nb_blocks    = 79 / block_size;
    const int max_nb_bytes = nb_blocks * (block_size - sizeof(struct plume_header));
    EXPECT_EQ(context.rb_number_bytes_total, max_nb_bytes);

    uint8_t buffer[max_nb_bytes + 20];
    for (int i = 0; i < max_nb_bytes; i ++) buffer[i] = i;
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header))), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_EDISK_FULL);

    plume_free_inmemory_driver(&driver);
}
TEST(WriterTests, TestPlumeTickDiskFails) {
    const int block_size = 7 + sizeof(plume_header);
    
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(6, block_size);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 79;
    
    plume_clear_disk(&context, &driver, 3);
    plume_open_write(&context);

    
    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;

    // number blocks in arena
    const int nb_blocks    = 79 / block_size;
    const int max_nb_bytes = nb_blocks * (block_size - sizeof(struct plume_header));
    EXPECT_EQ(context.rb_number_bytes_total, max_nb_bytes);

    uint8_t buffer[max_nb_bytes + 20];
    for (int i = 0; i < max_nb_bytes; i ++) buffer[i] = i;

    uint8_t order[3] = { PLUME_OK, PLUME_OK, PLUME_EMOCKED };
    inmem_driver->mock_buffers = order;
    inmem_driver->mock_buffers_size = 3;
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header))), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_EMOCKED);

    plume_free_inmemory_driver(&driver);
}
TEST(WriterTests, TestPlumeTickDMA) {
    const int block_size = 7 + sizeof(plume_header);
    
    struct plume_context context;
    struct plume_driver  driver = plume_allocate_inmemory_driver(6, block_size);

    uint8_t arena_buffer[1024];
    context.arena_buffer = arena_buffer;
    context.arena_length = 79;
    
    plume_clear_disk(&context, &driver, 3);
    plume_open_write(&context);

    struct plume_inmemory_driver* inmem_driver = 
        (struct plume_inmemory_driver*) driver.driver_ptr;

    // number blocks in arena
    const int nb_blocks    = 79 / block_size;
    const int max_nb_bytes = nb_blocks * (block_size - sizeof(struct plume_header));
    EXPECT_EQ(context.rb_number_bytes_total, max_nb_bytes);

    uint8_t buffer[max_nb_bytes + 20];
    for (int i = 0; i < max_nb_bytes; i ++) buffer[i] = i;

    uint8_t order_ready[3] = { 1, 0, 1 };
    inmem_driver->mock_ready = order_ready;
    inmem_driver->mock_ready_size = 3;
    EXPECT_EQ(plume_write(&context, buffer, max_nb_bytes - (block_size - sizeof(struct plume_header))), PLUME_OK);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[3 * block_size], PLUME_PAGE_FILESTART);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[4 * block_size], PLUME_PAGE_FILECONT);
    EXPECT_EQ(inmem_driver->buffer[5 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[5 * block_size], PLUME_PAGE_EMPTY0);
    EXPECT_EQ(plume_tick(&context), PLUME_OK);
    EXPECT_EQ(inmem_driver->buffer[5 * block_size], PLUME_PAGE_FILECONT);
    EXPECT_EQ(plume_tick(&context), PLUME_EDISK_FULL);

    plume_free_inmemory_driver(&driver);
}
