
#include <stdlib.h>

#include "plume/status.h"
#include "plume/drivers/inmemory.h"

uint8_t inmemory_disk_information (struct plume_inmemory_driver* driver_ptr, struct plume_disk* result) {
    if (driver_ptr == NULL) {
        return PLUME_ENULL;
    }
    if (result == NULL) {
        return PLUME_ENULL;
    }

    uint8_t mock_status = PLUME_OK;
    if (driver_ptr->mock_buffers_size) {
        mock_status = *driver_ptr->mock_buffers;

        driver_ptr->mock_buffers_size --;
        driver_ptr->mock_buffers ++;

        if (mock_status != PLUME_OK) {
            return mock_status;
        }
    }

    result->number_blocks = driver_ptr->block_count;
    result->block_size    = driver_ptr->block_size;

    return mock_status;
}

uint8_t inmemory_write_block_ready (
    struct plume_inmemory_driver* driver_ptr,
    struct plume_context* context
) {
    return 1;
}
uint8_t inmemory_write_block (
    struct plume_inmemory_driver* driver_ptr,
    struct plume_context* context,
    const uint8_t* buffer, 
    uint64_t block_id
) {
    // Driver shouldn't have been freed.
    if (driver_ptr == NULL) {
        return PLUME_ENULL;
    }
    
    if (block_id >= driver_ptr->block_count) {
        return PLUME_EOOB;
    }

    uint8_t mock_status = PLUME_OK;
    if (driver_ptr->mock_buffers_size) {
        mock_status = *driver_ptr->mock_buffers;

        driver_ptr->mock_buffers_size --;
        driver_ptr->mock_buffers ++;

        if (mock_status != PLUME_OK && mock_status != PLUME_OK_SENT_DMA) {
            return mock_status;
        }
    }

    uint64_t start  = block_id * driver_ptr->block_size;
    uint8_t* storage = driver_ptr->buffer + start;
    
    for (uint64_t offset = 0; offset < driver_ptr->block_size; offset ++) {
        storage[offset] = buffer[offset];
    }

    return mock_status;
}
uint8_t inmemory_read_block  (
    struct plume_inmemory_driver* driver_ptr,
    struct plume_context* context,
    uint8_t* buffer, 
    uint64_t block_id
) {
    // Driver shouldn't have been freed.
    if (driver_ptr == NULL) {
        return PLUME_ENULL;
    }
    
    if (block_id >= driver_ptr->block_count) {
        return PLUME_EOOB;
    }

    uint8_t mock_status = PLUME_OK;
    if (driver_ptr->mock_buffers_size) {
        mock_status = *driver_ptr->mock_buffers;

        driver_ptr->mock_buffers_size --;
        driver_ptr->mock_buffers ++;

        if (mock_status != PLUME_OK) {
            return mock_status;
        }
    }

    uint64_t start   = block_id * driver_ptr->block_size;
    uint8_t* storage = driver_ptr->buffer + start;
    
    for (uint64_t offset = 0; offset < driver_ptr->block_size; offset ++) {
        buffer[offset] = storage[offset];
    }

    return mock_status;
}

struct plume_driver plume_allocate_inmemory_driver ( uint64_t block_count, uint64_t block_size ) {
    struct plume_inmemory_driver* inmem_driver = malloc(sizeof(struct plume_inmemory_driver));
    inmem_driver->block_count = block_count;
    inmem_driver->block_size  = block_size;
    inmem_driver->buffer      = malloc(block_count * block_size);
    
    inmem_driver->mock_buffers      = NULL;
    inmem_driver->mock_buffers_size = 0;

    for (uint64_t offset = 0; offset < block_count * block_size; offset ++) {
        inmem_driver->buffer[offset] = 0;
    }

    struct plume_driver driver = {
        .driver_ptr = inmem_driver,

        /* Convert the virtual table to untypped functions */
        .disk_information =
            PLUME_DISK_INFORMATION_FN_TYPE
            &inmemory_disk_information,
        .read_block =
            PLUME_READ_BLOCK_FN_TYPE
            &inmemory_read_block,
        .write_block =
            PLUME_WRITE_BLOCK_FN_TYPE
            &inmemory_write_block,
        .write_block_ready = 
            PLUME_WRITE_BLOCK_READY_FN_TYPE
            &inmemory_write_block_ready
    };

    return driver;
}

void plume_free_inmemory_driver (struct plume_driver* driver) {
    struct plume_inmemory_driver* inmem_driver =
        (struct plume_inmemory_driver*) driver->driver_ptr;

    free(inmem_driver->buffer);
    free(inmem_driver);

    driver->driver_ptr = NULL;
}
