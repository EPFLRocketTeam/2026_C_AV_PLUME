
#include "plume/cli/entrypoint.h"
#include "plume/cli/command.h"
#include "plume/cli/reader.h"
#include "plume/header.h"
#include "plume/context.h"
#include "plume/status.h"
#include "../circular.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int plume_command_cat (struct plume_driver* driver, int argc, char** argv) {
    if (argc == 0) {
        help("ERROR: Missing <fileid> argument.");
        return 1;
    }

    int fileid = atoi(argv[0]);
    if (fileid < 0) {
        help("ERROR: File id should be a positive.");
        return 1;
    }

    struct plume_context context;
    context.arena_buffer = plume_cli_arena_buffer;
    context.arena_length = PLUME_CLI_ARENA_LENGTH;
    
    uint8_t status;
    status = plume_init(&context, driver);

    if (status != PLUME_OK) {
        help("ERROR: Could not initialize context, drive is likely invalid.");
        return status;
    }

    int nb_files = context.next_file_block - 1;

    if (nb_files <= 0) {
        fprintf(stderr, "No files on disk.\n");
        return 1;
    }

    if (fileid >= nb_files) {
        fprintf(stderr, "Invalid file ID, there are only %d files.\n", nb_files);
        return 1;
    }
    
    uint64_t* file_pointers;

    status = plume_get_file_pointers(&file_pointers, &context);
    if (status != PLUME_OK) {
        fprintf(stderr, "ERROR: Could not read the file pointers, drive is likely invalid.\n");
        return status;
    }
    
    uint64_t start = file_pointers[fileid];
    uint64_t end = file_pointers[fileid + 1];

    /* On a circular disk only the last data_blocks virtual blocks before the
     * head are still on disk: print what survives of the file. */
    uint64_t data_blocks = context.disk_info.number_blocks - context.fat_size;
    uint64_t from = start;
    uint64_t skipped = 0;
    if (context.circular) {
        uint64_t head   = file_pointers[nb_files];
        uint64_t oldest = head > context.fat_size + data_blocks ? head - data_blocks : context.fat_size;
        if (from < oldest) {
            from = oldest < end ? oldest : end;
        }
    }

    for (uint64_t idx = from; idx < end; idx ++) {
        uint64_t block = idx;
        uint32_t lap   = 1;
        if (context.circular) {
            block = context.fat_size + (idx - context.fat_size) % data_blocks;
            lap   = (uint32_t) (((idx - context.fat_size) / data_blocks + 1) & PLUME_LAP_MASK);
        }

        uint8_t status = context.driver->read_block(context.driver->driver_ptr, &context, context.arena_buffer, block);
        if (!plume_is_ok(status)) {
            free(file_pointers);
            fprintf(stderr, "ERROR: Could not read block %lu.\n", block);
            return status;
        }

        if (context.circular && plume_lap_read(context.arena_buffer + PLUME_HEADER_LAP_OFFSET) != lap) {
            skipped ++;
            continue;
        }

        status = plume_verify_block(context.arena_buffer, context.disk_info.block_size, idx == start);
        if (status != PLUME_OK) {
            free(file_pointers);
            fprintf(stderr, "ERROR: Could not validate block %lu.\n", block);
            return status;
        }

        write(
            STDOUT_FILENO,
            context.arena_buffer + sizeof(struct plume_header),
            context.disk_info.block_size - sizeof(struct plume_header)
        );
    }

    free(file_pointers);

    if (from >= end && end > start) {
        fprintf(stderr, "WARNING: all %lu blocks of file %d were overwritten.\n", end - start, fileid);
    } else if (from > start) {
        fprintf(stderr,
            "WARNING: the first %lu of the %lu blocks of file %d were overwritten; printed the last %lu, starting at block %lu of the disk.\n",
            from - start, end - start, fileid, end - from,
            context.fat_size + (from - context.fat_size) % data_blocks);
    }
    if (skipped > 0) {
        fprintf(stderr, "WARNING: skipped %lu blocks of file %d that belong to a later lap.\n", skipped, fileid);
    }

    return (from > start || skipped > 0) ? 1 : 0;
}
