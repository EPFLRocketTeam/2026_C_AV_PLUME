
#include <stddef.h>
#include "plume/context.h"
#include "plume/status.h"
#include "plume/const.h"

uint8_t read_page_settings (struct plume_context* context) {
    uint8_t error = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, 0);
    if (error != PLUME_OK) {
        return error;
    }
    
    if ((*context->arena_buffer) != PLUME_PAGE_SETTINGS) {
        return PLUME_EBAD_DISK;
    }

    context->fat_size = *((uint64_t*) (context->arena_buffer + 1));

    return PLUME_OK;
}
uint8_t is_page_empty (struct plume_context* context, uint8_t* result, uint64_t block_id) {
    uint8_t error = context->driver->read_block(context->driver->driver_ptr, context, context->arena_buffer, block_id);
    if (error != PLUME_OK) {
        return error;
    }

    *result = (*context->arena_buffer) == PLUME_PAGE_EMPTY0 || (*context->arena_buffer) == PLUME_PAGE_EMPTY1;
    return PLUME_OK;
}
uint8_t find_first_empty_page (
    struct plume_context* context,
    uint64_t* result, uint64_t start_block, uint64_t end_block
) {
    start_block --;
    end_block ++;

    while (end_block > start_block + 1) {
        uint64_t middle_block = (start_block + end_block) >> 1;

        uint8_t is_of_kind, error;
        error = is_page_empty(context, &is_of_kind, middle_block);
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

    error = read_page_settings(context);
    if (error != PLUME_OK) {
        return error;
    }
    
    error = find_first_empty_page(
        context,
        &context->next_file_block,
        1, context->fat_size - 1
    );
    if (error != PLUME_OK) {
        return error;
    }
    
    error = find_first_empty_page(
        context,
        &context->next_valid_block,
        context->fat_size,
        context->disk_info.number_blocks - 1
    );
    if (error != PLUME_OK) {
        return error;
    }
    
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

    uint64_t* int_buffer = (uint64_t*) (context->arena_buffer + 1);
    *int_buffer = fat_size;

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
    context->next_file_block = 0;
    context->next_valid_block = fat_size;

    return PLUME_OK;
}

