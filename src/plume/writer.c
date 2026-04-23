
#include <stddef.h>
#include "plume/const.h"
#include "plume/status.h"
#include "plume/driver.h"
#include "plume/writer.h"
#include "plume/header.h"

uint8_t plume_open_write (struct plume_context* context) {
    if (context == NULL || context->driver == NULL) {
        return PLUME_ENULL;
    }

    if (context->next_file_block == context->fat_size) {
        return PLUME_EFAT_FULL;
    }

    context->arena_buffer[0] = PLUME_PAGE_FILEINFO;
    
    uint64_t* int_buffer = (uint64_t*) (context->arena_buffer + 1);
    *int_buffer = context->next_valid_block;

    uint8_t error = plume_write_block_blocking(context, context->arena_buffer, context->next_file_block);
    if (!plume_is_ok(error)) {
        return error;
    }

    context->next_file_block ++;
    context->is_first_block = 1;

    return PLUME_OK;
}
uint8_t plume_write (struct plume_context* context, const uint8_t* buffer, uint64_t size) {
    if (context == NULL || context->driver == NULL) {
        return PLUME_ENULL;
    }

    if (size > context->rb_max_nb_bytes_per_write) {
        return PLUME_EARENA_TOO_SMALL;
    }

    if (context->rb_number_bytes_used + size > context->rb_number_bytes_total) {
        return PLUME_OK_RETRY;
    }

    for (size_t offset = 0; offset < size; offset ++) {
        context->rb_number_bytes_used ++;

        context->arena_buffer[context->rb_rgt_block * context->disk_info.block_size + context->rb_rgt_offset] = buffer[offset];
        context->rb_rgt_offset ++;

        if (context->rb_rgt_offset == context->disk_info.block_size) {
            context->rb_rgt_block ++;
            context->rb_rgt_offset = sizeof(struct plume_header);
            context->rb_number_blocks_used ++;

            if (context->rb_rgt_block == context->rb_number_blocks) {
                context->rb_rgt_block = 0;
            }
        }
    }

    return PLUME_OK;
}
uint8_t plume_tick (struct plume_context* context) {
    if (context == NULL || context->driver == NULL) {
        return PLUME_ENULL;
    }

    if (context->rb_pending_write) { 
        if (context->driver->write_block_ready(context->driver->driver_ptr, context)) {
            context->rb_pending_write = 0;
            context->rb_lft_block ++;
            context->rb_number_blocks_used --;
            context->rb_number_bytes_used -= context->disk_info.block_size - sizeof(struct plume_header);
            context->next_valid_block ++;

            if (context->rb_lft_block == context->rb_number_blocks) {
                context->rb_lft_block = 0;
            }
        } else {
            return PLUME_OK;
        }
    }

    if (context->rb_number_blocks_used == 0) {
        return PLUME_OK;
    }

    if (context->next_valid_block >= context->disk_info.number_blocks) {
        return PLUME_EDISK_FULL;
    }

    uint8_t* block = context->arena_buffer + (context->rb_lft_block * context->disk_info.block_size);
    plume_prepare_block(block, context->disk_info.block_size, context->is_first_block);
    context->is_first_block = 0;

    uint8_t status = context->driver->write_block(
        context->driver->driver_ptr,
        context,
        block,
        context->next_valid_block
    );

    if (plume_is_ok(status)) {
        context->rb_pending_write = 1;
    }

    return status;
}
