
#include <stddef.h>
#include <string.h>
#include "plume/context.h"
#include "plume/status.h"
#include "plume/header.h"
#include "plume/const.h"

void plume_prepare_context (struct plume_context* context) {
    context->rb_lft_block = 0;
    context->rb_rgt_block = 0;
    context->rb_rgt_offset = sizeof(struct plume_header);
    context->rb_number_blocks = context->arena_length / context->disk_info.block_size;
    context->rb_number_bytes_total = context->rb_number_blocks * (context->disk_info.block_size - sizeof(struct plume_header));
    context->rb_max_nb_bytes_per_write = (context->rb_number_blocks - 1) * (context->disk_info.block_size - sizeof(struct plume_header));
    context->rb_number_bytes_used = 0;
    context->rb_number_blocks_used = 0;
    context->rb_pending_write = 0;
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
    
    error = plume_find_first_empty_page(
        context,
        &context->next_valid_block,
        context->fat_size,
        context->disk_info.number_blocks - 1
    );
    if (error != PLUME_OK) {
        return error;
    }
    
    plume_prepare_context(context);
    return PLUME_OK;
}

uint8_t plume_clear_disk (
    struct plume_context* context,
    struct plume_driver*  driver,
    uint64_t fat_size
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

    for (uint64_t offset = 0; offset < context->disk_info.block_size; offset ++) {
        context->arena_buffer[offset] = PLUME_PAGE_EMPTY0;
    }

    *(context->arena_buffer) = PLUME_PAGE_SETTINGS;

    memcpy(context->arena_buffer + 1, &fat_size, sizeof(uint64_t));

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

    plume_prepare_context(context);
    return PLUME_OK;
}

