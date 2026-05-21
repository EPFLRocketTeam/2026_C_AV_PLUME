
#ifndef PLUME_ATOMIC_H
#define PLUME_ATOMIC_H

#include "plume/context.h"

struct plume_snapshot {
    // Write - Ring Buffer
    uint64_t rb_number_blocks;
    uint64_t rb_number_bytes_total;
    uint64_t rb_max_nb_bytes_per_write;
    uint64_t rb_number_blocks_used;

    uint64_t rb_number_bytes_used;
    uint64_t rb_lft_block;
    uint64_t rb_rgt_block;
    uint64_t rb_rgt_offset;

    uint8_t rb_pending_batch_size;
    uint8_t is_first_block;
};

struct plume_snapshot plume_save (struct plume_context* context);

void plume_rollback (struct plume_context* context, struct plume_snapshot *snapshot);

#endif /* PLUME_ATOMIC_H */
