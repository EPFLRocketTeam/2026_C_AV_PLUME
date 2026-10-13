
#include "plume/cli/reader.h"
#include "plume/status.h"
#include "../circular.h"
#include <malloc.h>

/* On a circular disk the pointers are virtual blocks: blocks counted since the
 * disk was cleared, (lap - 1) * data_blocks + block. During the first lap, and
 * on every linear disk, that is the block itself. */
static uint64_t plume_virtual_block (struct plume_context* context, uint64_t block, uint32_t lap) {
    if (!context->circular) {
        return block;
    }
    uint64_t data_blocks = context->disk_info.number_blocks - context->fat_size;
    return (uint64_t) (lap - 1) * data_blocks + block;
}

int plume_get_file_pointers (uint64_t** result, struct plume_context* context) {
    uint64_t* pointers = (uint64_t*) malloc(context->next_file_block * sizeof(uint64_t));

    for (uint64_t offset = 1; offset < context->next_file_block; offset ++) {
        int status = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, offset);
        if (!plume_is_ok(status)) {
            free(pointers);
            return status;
        }

        pointers[offset - 1] = plume_virtual_block(
            context,
            *((uint64_t*) (context->arena_buffer + 1)),
            plume_lap_read(context->arena_buffer + PLUME_FILEINFO_LAP_OFFSET)
        );
    }
    pointers[context->next_file_block - 1] = plume_virtual_block(context, context->next_valid_block, context->lap);

    *result = pointers;

    return PLUME_OK;
}
