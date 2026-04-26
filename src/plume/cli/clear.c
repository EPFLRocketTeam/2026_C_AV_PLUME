
#include "plume/cli/entrypoint.h"
#include "plume/cli/command.h"
#include "plume/cli/reader.h"
#include "plume/header.h"
#include "plume/context.h"
#include "plume/status.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

int plume_command_clear (struct plume_driver* driver, int argc, char** argv) {
    if (argc == 0) {
        help("ERROR: Missing <fatsize> argument.");
        return 1;
    }

    int fatsize = atoi(argv[0]);
    if (fatsize <= 0) {
        help("ERROR: Fatsize should be a strictly positive integer.");
        return 1;
    }

    printf("Clearing disk with fat size %d...\n", fatsize);

    struct plume_context context;
    context.arena_buffer = plume_cli_arena_buffer;
    context.arena_length = PLUME_CLI_ARENA_LENGTH;
    
    uint8_t status;
    status = plume_clear_disk(&context, driver, fatsize);
    if (status != PLUME_OK) {
        printf("ERROR: Could not clear disk, device is likely invalid.\n");
        return status;
    }

    printf("Drive cleared successfully.\n");
    return 0;
}
