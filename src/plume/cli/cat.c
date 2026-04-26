
#include "plume/cli/entrypoint.h"
#include "plume/cli/command.h"
#include "plume/cli/reader.h"
#include "plume/header.h"
#include "plume/context.h"
#include "plume/status.h"
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
        return 0;
    }

    if (fileid >= nb_files) {
        fprintf(stderr, "Invalid file ID, there are only %d files.\n", nb_files);
        return 0;
    }
    
    uint64_t* file_pointers;

    status = plume_get_file_pointers(&file_pointers, &context);
    if (status != PLUME_OK) {
        fprintf(stderr, "ERROR: Could not read the file pointers, drive is likely invalid.\n");
        return status;
    }
    
    uint64_t start = file_pointers[fileid];
    uint64_t end = file_pointers[fileid + 1];

    for (uint64_t idx = start; idx < end; idx ++) {
        uint8_t status = context.driver->read_block(context.driver->driver_ptr, &context, context.arena_buffer, idx);
        if (!plume_is_ok(status)) {
            free(file_pointers);
            fprintf(stderr, "ERROR: Could not read block %lu.\n", idx);
            return status;
        }

        status = plume_verify_block(context.arena_buffer, context.disk_info.block_size, idx == start);
        if (status != PLUME_OK) {
            free(file_pointers);
            fprintf(stderr, "ERROR: Could not validate block %lu.\n", idx);
            return status;
        }

        write(
            STDOUT_FILENO,
            context.arena_buffer + sizeof(struct plume_header),
            context.disk_info.block_size - sizeof(struct plume_header)
        );
    }

    free(file_pointers);

    return 0;
}
