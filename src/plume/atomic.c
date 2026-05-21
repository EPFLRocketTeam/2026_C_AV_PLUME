
#include "plume/atomic.h"

struct plume_snapshot plume_save (struct plume_context* context) {
    struct plume_snapshot snapshot;
    
    snapshot.rb_number_blocks = context->rb_number_blocks;
    snapshot.rb_number_bytes_total = context->rb_number_bytes_total;
    snapshot.rb_max_nb_bytes_per_write = context->rb_max_nb_bytes_per_write;
    snapshot.rb_number_blocks_used = context->rb_number_blocks_used;

    snapshot.rb_number_bytes_used = context->rb_number_bytes_used;
    snapshot.rb_lft_block = context->rb_lft_block;
    snapshot.rb_rgt_block = context->rb_rgt_block;
    snapshot.rb_rgt_offset = context->rb_rgt_offset;

    snapshot.rb_pending_batch_size = context->rb_pending_batch_size;
    snapshot.is_first_block = context->is_first_block;
    
    return snapshot;
}

void plume_rollback (struct plume_context* context, struct plume_snapshot *snapshot) {
    context->rb_number_blocks = snapshot->rb_number_blocks;
    context->rb_number_bytes_total = snapshot->rb_number_bytes_total;
    context->rb_max_nb_bytes_per_write = snapshot->rb_max_nb_bytes_per_write;
    context->rb_number_blocks_used = snapshot->rb_number_blocks_used;

    context->rb_number_bytes_used = snapshot->rb_number_bytes_used;
    context->rb_lft_block = snapshot->rb_lft_block;
    context->rb_rgt_block = snapshot->rb_rgt_block;
    context->rb_rgt_offset = snapshot->rb_rgt_offset;

    context->rb_pending_batch_size = snapshot->rb_pending_batch_size;
    context->is_first_block = snapshot->is_first_block;
}

