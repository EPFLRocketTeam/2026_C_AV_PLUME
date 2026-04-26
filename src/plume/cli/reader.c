
#include "plume/cli/reader.h"
#include "plume/status.h"
#include <malloc.h>

int plume_get_file_pointers (uint64_t** result, struct plume_context* context) {
    uint64_t* pointers = (uint64_t*) malloc(context->next_file_block * sizeof(uint64_t));

    for (uint64_t offset = 1; offset < context->next_file_block; offset ++) {
        int status = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, offset);
        if (!plume_is_ok(status)) {
            free(pointers);
            return status;
        }

        pointers[offset - 1] = *((uint64_t*) (context->arena_buffer + 1));
    }
    pointers[context->next_file_block - 1] = context->next_valid_block;

    *result = pointers;

    return PLUME_OK;
}
