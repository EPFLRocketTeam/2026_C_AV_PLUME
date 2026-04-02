
#ifndef PLUME_INMEMORY_DRIVER_H
#define PLUME_INMEMORY_DRIVER_H

#include <stdint.h>

#include "plume/driver.h"

struct plume_inmemory_driver {
    uint8_t* buffer;

    uint64_t block_count;
    uint64_t block_size;

    uint8_t* mock_buffers;
    uint64_t mock_buffers_size;
};

struct plume_driver plume_allocate_inmemory_driver ( uint64_t block_count, uint64_t block_size );
void plume_free_inmemory_driver (struct plume_driver* driver);

#endif /* PLUME_INMEMORY_DRIVER_H */
