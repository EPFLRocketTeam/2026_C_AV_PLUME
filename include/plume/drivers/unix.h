
#ifndef PLUME_UNIX_DRIVER_H
#define PLUME_UNIX_DRIVER_H

#include <stdint.h>

#include "plume/driver.h"

struct plume_unix_driver {
    int      fd;
    uint64_t block_size;
    uint64_t block_count;
    uint64_t current_block;
};

int plume_allocate_unix_driver (struct plume_driver* out, const char* device);
void plume_free_unix_driver (struct plume_driver* driver);

#endif /* PLUME_INMEMORY_DRIVER_H */
