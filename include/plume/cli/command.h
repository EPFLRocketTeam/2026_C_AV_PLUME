
#ifndef PLUME_COMMAND_H
#define PLUME_COMMAND_H

#include "plume/driver.h"

struct plume_command {
    const char* name;
    const char* usage;
    const char* description;
    
    int (*call_command)(struct plume_driver* driver, int argc, char** argv);
};

int plume_command_clear (struct plume_driver* driver, int argc, char** argv);
int plume_command_ls    (struct plume_driver* driver, int argc, char** argv);
int plume_command_cat   (struct plume_driver* driver, int argc, char** argv);

extern const uint64_t plume_commands_count;

extern const struct plume_command  plume_commands_list[];
extern const struct plume_command* plume_commands_list_end;

#define PLUME_CLI_ARENA_LENGTH 64 * 1024

extern uint8_t plume_cli_arena_buffer[PLUME_CLI_ARENA_LENGTH];

#define PLUME_FOR_COMMAND(command) \
    for ( \
        const struct plume_command* command = plume_commands_list; \
        command != plume_commands_list_end; \
        command ++)

#endif
