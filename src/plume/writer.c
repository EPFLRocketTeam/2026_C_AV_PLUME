
#include <stddef.h>
#include <string.h>
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
    
    memcpy(context->arena_buffer + 1, &context->next_valid_block, sizeof(uint64_t));

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

    /* ── Optimised bulk copy ───────────────────────────────────────────
     * Instead of writing one byte at a time (very slow to non-cacheable
     * RAM_D2), copy as large a contiguous chunk as fits in the current
     * block's remaining payload, then advance to the next block.        */
    while (size > 0) {
        uint32_t dst = (uint32_t)(context->rb_rgt_block * context->disk_info.block_size
                                  + context->rb_rgt_offset);
        uint32_t space = (uint32_t)(context->disk_info.block_size - context->rb_rgt_offset);
        uint32_t chunk = ((uint32_t)size < space) ? (uint32_t)size : space;

        memcpy(context->arena_buffer + dst, buffer, chunk);
        buffer += chunk;
        size   -= chunk;
        context->rb_rgt_offset        += chunk;
        context->rb_number_bytes_used += chunk;

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

    if (context->rb_pending_batch_size > 0) { 
        if (context->driver->write_block_ready(context->driver->driver_ptr, context)) {
            uint8_t batch = context->rb_pending_batch_size;
            context->rb_pending_batch_size = 0;
            context->rb_lft_block += batch;
            context->rb_number_blocks_used -= batch;
            context->rb_number_bytes_used -= batch * (context->disk_info.block_size - sizeof(struct plume_header));
            context->next_valid_block += batch;

            if (context->rb_lft_block >= context->rb_number_blocks) {
                context->rb_lft_block -= context->rb_number_blocks;
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

    /* Compute how many contiguous blocks we can flush in one transfer. */
    uint32_t batch_size = context->rb_number_blocks_used;

    /* Cannot write past the ring-buffer wrap boundary (memory contiguity). */
    uint64_t blocks_until_wrap = context->rb_number_blocks - context->rb_lft_block;
    if (batch_size > blocks_until_wrap) {
        batch_size = (uint32_t) blocks_until_wrap;
    }

    /* Cannot write past the disk end. */
    uint64_t blocks_until_disk_end = context->disk_info.number_blocks - context->next_valid_block;
    if (batch_size > blocks_until_disk_end) {
        batch_size = (uint32_t) blocks_until_disk_end;
    }

    /* Cap at configured maximum. */
    if (batch_size > PLUME_MAX_BATCH_SIZE) {
        batch_size = PLUME_MAX_BATCH_SIZE;
    }

    /* Prepare headers for every block in the batch. */
    for (uint32_t i = 0; i < batch_size; i++) {
        uint8_t* blk = context->arena_buffer + ((context->rb_lft_block + i) * context->disk_info.block_size);
        plume_prepare_block(blk, context->disk_info.block_size, context->is_first_block && (i == 0));
    }
    context->is_first_block = 0;

    uint8_t* block = context->arena_buffer + (context->rb_lft_block * context->disk_info.block_size);
    uint8_t status;

    if (context->driver->write_blocks != NULL && batch_size > 1) {
        status = context->driver->write_blocks(
            context->driver->driver_ptr,
            context,
            block,
            context->next_valid_block,
            batch_size
        );
    } else {
        /* Fall back to single-block write. */
        batch_size = 1;
        status = context->driver->write_block(
            context->driver->driver_ptr,
            context,
            block,
            context->next_valid_block
        );
    }

    if (plume_is_ok(status)) {
        context->rb_pending_batch_size = (uint8_t) batch_size;
    }

    return status;
}
