
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

    uint64_t arena_length;
    uint8_t* arena_buffer;
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
