
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
    
    /* On a circular disk, blocks older than one data area before the head
     * were overwritten: SIZE counts the blocks that survive. */
    uint64_t oldest = 0;
    if (context.circular) {
        uint64_t data_blocks = context.disk_info.number_blocks - context.fat_size;
        uint64_t head        = file_pointers[nb_files];
        oldest = head > context.fat_size + data_blocks ? head - data_blocks : context.fat_size;
    }

    printf(    "INDEX   SIZE\n");
    for (int file_id = 0; file_id < nb_files; file_id ++) {
        uint64_t start = file_pointers[file_id];
        uint64_t end   = file_pointers[file_id + 1];

        printf("%-5d", file_id);
        if (start < oldest) {
            uint64_t from = oldest < end ? oldest : end;
            printf(     "   %lu (first %lu blocks overwritten)\n", end - from, from - start);
        } else {
            printf(     "   %lu\n", end - start);
        }
    }

    free(file_pointers);
    return 0;
}