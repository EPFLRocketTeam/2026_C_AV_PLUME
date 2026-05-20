
#ifndef PLUME_CONTEXT_H
#define PLUME_CONTEXT_H

struct plume_context;

#include "plume/driver.h"
#include "plume/disk.h"

struct plume_context {
    struct plume_driver* driver;

    struct plume_disk disk_info;

    uint64_t fat_size;
    uint64_t next_file_block;
    uint64_t next_valid_block;

    // Memory Arena
    uint64_t arena_length;
    uint8_t* arena_buffer;

    // Write - Ring Buffer
    uint64_t rb_number_blocks;
    uint64_t rb_number_bytes_total;
    uint64_t rb_max_nb_bytes_per_write;
    uint64_t rb_number_blocks_used;

    uint64_t rb_number_bytes_used;
    uint64_t rb_lft_block, rb_rgt_block;
    uint64_t rb_rgt_offset;

    uint8_t rb_pending_batch_size;
    uint8_t is_first_block;
};

uint8_t plume_init (
    struct plume_context* context,
    struct plume_driver*  driver
);

uint8_t plume_clear_disk (
    struct plume_context* context,
    struct plume_driver*  driver,
    uint64_t fat_size
);

#endif /* PLUME_CONTEXT_H */
