
#ifndef PLUME_CONST_H
#define PLUME_CONST_H

#define PLUME_PAGE_EMPTY0    0b00000000
#define PLUME_PAGE_EMPTY1    0b11111111
#define PLUME_PAGE_SETTINGS  0b11001100
#define PLUME_PAGE_FILEINFO  0b00110011
#define PLUME_PAGE_FILESTART 0b10101010
#define PLUME_PAGE_FILECONT  0b01010101

/* Maximum number of contiguous blocks flushed per plume_tick() call.
 * 64 blocks × 512 B = 32 KB — larger batches reduce per-block overhead
 * and match the 256KB arena (512 blocks).  CMD23 pre-erase sent for
 * multi-block writes further improves card internal scheduling. */
#ifndef PLUME_MAX_BATCH_SIZE
#define PLUME_MAX_BATCH_SIZE 64
#endif

#endif /* PLUME_CONST_H */
