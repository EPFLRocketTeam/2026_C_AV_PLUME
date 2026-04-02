
#include "plume/driver.h"
#include "plume/status.h"

uint8_t plume_write_block_blocking (struct plume_context* context, const uint8_t* buffer, uint64_t block_id) {
    uint8_t error;

    do {
        error = context->driver->write_block(
            context->driver->driver_ptr,
            context,
            buffer,
            block_id
        );
    } while (error == PLUME_OK_RETRY);

    return error;
}
