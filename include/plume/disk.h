
#ifndef PLUME_DISK_H
#define PLUME_DISK_H

#include <stdint.h>

struct plume_disk {
    uint64_t number_blocks;
    uint64_t block_size;
};

#endif /* PLUME_DISK_H */
