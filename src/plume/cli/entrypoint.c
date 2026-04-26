
#include <stdio.h>
#include <string.h>
#include "plume/cli/command.h"
#include "plume/cli/entrypoint.h"
#include "plume/drivers/unix.h"
#include "plume/status.h"

void help (const char* info_message) {
    fprintf(stderr, "Plume Software © EPFL Rocket Team 2026\n");
    fprintf(stderr, "Usage: ./plume_cli <device> <command> <args...>\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Available commands: \n");

    int max_usage_length = 0;
    PLUME_FOR_COMMAND(command) {
        int command_usage = strlen(command->usage);

        if (max_usage_length < command_usage) {
            max_usage_length = command_usage;
        }
    }

    int usage_with_padding = max_usage_length + 3;
    PLUME_FOR_COMMAND(command) {
        fprintf(stderr, " - %-*s %s\n", usage_with_padding, command->usage, command->description);
    }
    
    if (info_message != NULL) {
        fprintf(stderr, "\n");
        fprintf(stderr, "%s\n", info_message);
    }
}

int entrypoint (int argc, char** argv) {
    if (argc <= 0) {
        help("ERROR: Missing device.");
        return 1;
    }

    struct plume_driver driver;

    int status = plume_allocate_unix_driver(&driver, argv[0]);
    if (status != PLUME_OK) {
        help("ERROR: Could not initialize driver.");
        return status;
    }

    argc --;
    argv ++;

    if (argc == 0) {
        help("ERROR: Missing command.");
        return 1;
    }

    PLUME_FOR_COMMAND(command) {
        if (!strcmp(argv[0], command->name)) {
            return command->call_command(&driver, argc - 1, argv + 1);
        }
    }

    help("ERROR: Unknown command.");
    return 0;
}
