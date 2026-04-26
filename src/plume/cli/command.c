
#include "plume/cli/command.h"

const uint64_t plume_commands_count = 3;
const struct plume_command plume_commands_list[] = {
    { "clear", "clear <fatsize>", "Clear the disk.",                   &plume_command_clear },
    { "ls",    "ls",              "List files inside the disk.",       &plume_command_ls },
    { "cat",   "cat <file-id>",   "Cat the file at the given index .", &plume_command_cat }
};
const struct plume_command* plume_commands_list_end = plume_commands_list + plume_commands_count;

uint8_t plume_cli_arena_buffer[PLUME_CLI_ARENA_LENGTH];
