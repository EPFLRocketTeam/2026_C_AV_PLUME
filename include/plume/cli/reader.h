
#include "plume/context.h"

/* Returns next_file_block pointers: the first block of every file, then the
 * write head, so file i spans [result[i], result[i + 1]). On a circular disk
 * they are virtual blocks, (lap - 1) * data blocks + block, which equal the
 * block itself during the first lap and on every linear disk. */
int plume_get_file_pointers (uint64_t** result, struct plume_context* context);
