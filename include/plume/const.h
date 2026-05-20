
#ifndef PLUME_CONST_H
#define PLUME_CONST_H

#define PLUME_PAGE_EMPTY0    0b00000000
#define PLUME_PAGE_EMPTY1    0b11111111
#define PLUME_PAGE_SETTINGS  0b11001100
#define PLUME_PAGE_FILEINFO  0b00110011
#define PLUME_PAGE_FILESTART 0b10101010
#define PLUME_PAGE_FILECONT  0b01010101

/* Maximum number of contiguous blocks flushed per plume_tick() call.
 * 32 blocks × 512 B = 16 KB — sweet-spot for SDMMC multi-block DMA
 * throughput on STM32H7 (benchmarked at ~9 MB/s). */
#ifndef PLUME_MAX_BATCH_SIZE
#define PLUME_MAX_BATCH_SIZE 32
#endif

#endif /* PLUME_CONST_H */
