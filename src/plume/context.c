
#include <stddef.h>
#include <string.h>
#include "plume/context.h"
#include "plume/status.h"
#include "plume/header.h"
#include "plume/const.h"
#include "circular.h"

void plume_prepare_context (struct plume_context* context) {
    context->rb_lft_block = 0;
    context->rb_rgt_block = 0;
    context->rb_rgt_offset = sizeof(struct plume_header);
    context->rb_number_blocks = context->arena_length / context->disk_info.block_size;
    context->rb_number_bytes_total = context->rb_number_blocks * (context->disk_info.block_size - sizeof(struct plume_header));
    context->rb_max_nb_bytes_per_write = (context->rb_number_blocks - 1) * (context->disk_info.block_size - sizeof(struct plume_header));
    context->rb_number_bytes_used = 0;
    context->rb_number_blocks_used = 0;
    context->rb_pending_batch_size = 0;
}

uint8_t plume_read_page_settings (struct plume_context* context) {
    uint8_t error = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, 0);
    if (error != PLUME_OK) {
        return error;
    }
    
    if ((*context->arena_buffer) != PLUME_PAGE_SETTINGS) {
        return PLUME_EBAD_DISK;
    }

    memcpy(&context->fat_size, context->arena_buffer + 1, sizeof(uint64_t));
    context->circular = context->disk_info.block_size > PLUME_SETTINGS_FLAGS_OFFSET
        && (context->arena_buffer[PLUME_SETTINGS_FLAGS_OFFSET] & PLUME_SETTINGS_CIRCULAR) != 0;

    return PLUME_OK;
}
uint8_t plume_is_page_empty (struct plume_context* context, uint8_t* result, uint64_t block_id) {
    uint8_t error = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, block_id);
    if (error != PLUME_OK) {
        return error;
    }

    *result = (*context->arena_buffer) == PLUME_PAGE_EMPTY0 || (*context->arena_buffer) == PLUME_PAGE_EMPTY1;
    return PLUME_OK;
}
uint8_t plume_find_first_empty_page (
    struct plume_context* context,
    uint64_t* result, uint64_t start_block, uint64_t end_block
) {
    start_block --;
    end_block ++;

    while (end_block > start_block + 1) {
        uint64_t middle_block = (start_block + end_block) >> 1;

        uint8_t is_of_kind, error;
        error = plume_is_page_empty(context, &is_of_kind, middle_block);
        if (error != PLUME_OK) {
            return error;
        }

        if (is_of_kind) {
            end_block = middle_block;
        } else {
            start_block = middle_block;
        }
    }

    *result = end_block;

    return PLUME_OK;
}

/* Finds the write head of a circular disk. The data area holds pages of the
 * current lap L from fat_size up to the head, then pages of lap L - 1 (or
 * empty pages during the first lap) up to the disk end, so "empty or not of
 * lap L" is false then true, and the head is its first true block. */
static uint8_t plume_find_circular_head (struct plume_context* context) {
    uint64_t first = context->fat_size;
    uint64_t end   = context->disk_info.number_blocks;

    uint8_t empty;
    uint8_t error = plume_is_page_empty(context, &empty, first);
    if (error != PLUME_OK) {
        return error;
    }
    if (empty) {
        context->next_valid_block = first;
        context->lap = 1;
        return PLUME_OK;
    }

    uint32_t lap = plume_lap_read(context->arena_buffer + PLUME_HEADER_LAP_OFFSET);

    uint64_t lo = first, hi = end;
    while (hi > lo + 1) {
        uint64_t middle = (lo + hi) >> 1;

        error = plume_is_page_empty(context, &empty, middle);
        if (error != PLUME_OK) {
            return error;
        }

        if (empty || plume_lap_read(context->arena_buffer + PLUME_HEADER_LAP_OFFSET) != lap) {
            hi = middle;
        } else {
            lo = middle;
        }
    }

    if (hi == end) {
        /* The last lap ended exactly at the disk end. */
        context->next_valid_block = first;
        context->lap = plume_lap_next(lap);
    } else {
        context->next_valid_block = hi;
        context->lap = lap;
    }
    return PLUME_OK;
}

uint8_t plume_init (struct plume_context* context, struct plume_driver* driver) {
    if (context == NULL || driver == NULL) {
        return PLUME_ENULL;
    }

    context->driver = driver;
    uint8_t error = context->driver->disk_information(driver->driver_ptr, &context->disk_info);
    if (error != PLUME_OK) {
        return error;
    }

    if (context->disk_info.block_size > context->arena_length) {
        return PLUME_EARENA_TOO_SMALL;
    }

    error = plume_read_page_settings(context);
    if (error != PLUME_OK) {
        return error;
    }
    
    error = plume_find_first_empty_page(
        context,
        &context->next_file_block,
        1, context->fat_size - 1
    );
    if (error != PLUME_OK) {
        return error;
    }
    
    if (context->circular) {
        if (context->fat_size >= context->disk_info.number_blocks) {
            return PLUME_EBAD_DISK;
        }
        error = plume_find_circular_head(context);
    } else {
        context->lap = 1;
        error = plume_find_first_empty_page(
            context,
            &context->next_valid_block,
            context->fat_size,
            context->disk_info.number_blocks - 1
        );
    }
    if (error != PLUME_OK) {
        return error;
    }
    
    plume_prepare_context(context);
    return PLUME_OK;
}

static uint8_t plume_clear_disk_mode (
    struct plume_context* context,
    struct plume_driver*  driver,
    uint64_t fat_size,
    uint8_t  flags
) {
    if (context == NULL || driver == NULL) {
        return PLUME_ENULL;
    }

    context->driver = driver;
    
    uint8_t error = driver->disk_information(driver->driver_ptr, &context->disk_info);
    if (!plume_is_ok(error)) {
        return error;
    }

    if (context->disk_info.block_size > context->arena_length) {
        return PLUME_EARENA_TOO_SMALL;
    }

    /* plume_init searches the FAT up to fat_size - 1: a FAT past the end of
     * the disk would leave a settings page that no reader can open. */
    if (fat_size == 0 || fat_size > context->disk_info.number_blocks) {
        return PLUME_EOOB;
    }
    /* A circular disk wraps over its data area, which cannot be empty. */
    if ((flags & PLUME_SETTINGS_CIRCULAR) && fat_size == context->disk_info.number_blocks) {
        return PLUME_EOOB;
    }

    for (uint64_t offset = 0; offset < context->disk_info.block_size; offset ++) {
        context->arena_buffer[offset] = PLUME_PAGE_EMPTY0;
    }

    *(context->arena_buffer) = PLUME_PAGE_SETTINGS;

    memcpy(context->arena_buffer + 1, &fat_size, sizeof(uint64_t));
    if (flags) {
        context->arena_buffer[PLUME_SETTINGS_FLAGS_OFFSET] = flags;
    }

    error = plume_write_block_blocking(context, context->arena_buffer, 0);
    if (!plume_is_ok(error)) {
        return error;
    }

    for (uint64_t offset = 0; offset < context->disk_info.block_size; offset ++) {
        context->arena_buffer[offset] = PLUME_PAGE_EMPTY0;
    }

    for (uint64_t offset = 1; offset < context->disk_info.number_blocks; offset ++) {
        error = plume_write_block_blocking(context, context->arena_buffer, offset);
        
        if (!plume_is_ok(error)) {
            return error;
        }
    }
    
    context->fat_size = fat_size;
    context->next_file_block = 1;
    context->next_valid_block = fat_size;
    context->circular = (flags & PLUME_SETTINGS_CIRCULAR) != 0;
    context->lap = 1;

    plume_prepare_context(context);
    return PLUME_OK;
}

uint8_t plume_clear_disk (
    struct plume_context* context,
    struct plume_driver*  driver,
    uint64_t fat_size
) {
    return plume_clear_disk_mode(context, driver, fat_size, 0);
}

uint8_t plume_clear_disk_circular (
    struct plume_context* context,
    struct plume_driver*  driver,
    uint64_t fat_size
) {
    return plume_clear_disk_mode(context, driver, fat_size, PLUME_SETTINGS_CIRCULAR);
}

