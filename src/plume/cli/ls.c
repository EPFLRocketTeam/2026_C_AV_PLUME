
#include "plume/cli/entrypoint.h"
#include "plume/cli/command.h"
#include "plume/cli/reader.h"
#include "plume/context.h"
#include "plume/status.h"
#include <stdlib.h>
#include <stdio.h>

int plume_command_ls (struct plume_driver* driver, int argc, char** argv) {
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
        printf("No files on disk.\n");
        return 0;
    }

    printf(
        nb_files == 1 ? "There is %d file on the disk.\n\n"
                      : "There are %d files on the disk.\n\n", nb_files);
    
    uint64_t* file_pointers;

    status = plume_get_file_pointers(&file_pointers, &context);
    if (status != PLUME_OK) {
        fprintf(stderr, "ERROR: Could not read the file pointers, drive is likely invalid.\n");
        return status;
    }
    
    printf(    "INDEX   SIZE\n");
    for (int file_id = 0; file_id < nb_files; file_id ++) {
        printf("%-5d", file_id);
        printf(     "   %lu\n", file_pointers[file_id + 1] - file_pointers[file_id]);
    }

    free(file_pointers);
    return 0;
}