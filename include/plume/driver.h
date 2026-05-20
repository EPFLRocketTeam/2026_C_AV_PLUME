
#ifndef PLUME_DRIVER_H
#define PLUME_DRIVER_H

#include <stdint.h>

struct plume_driver;

#include "plume/disk.h"
#include "plume/context.h"

#define PLUME_DISK_INFORMATION_FN_TYPE (uint8_t (*)(void*, struct plume_disk*))
#define PLUME_READ_BLOCK_FN_TYPE (uint8_t (*)(void*, struct plume_context*, uint8_t*, uint64_t))
#define PLUME_WRITE_BLOCK_FN_TYPE (uint8_t (*)(void*, struct plume_context*, const uint8_t*, uint64_t))
#define PLUME_WRITE_BLOCK_READY_FN_TYPE (uint8_t (*)(void*, struct plume_context*))
#define PLUME_WRITE_BLOCKS_FN_TYPE (uint8_t (*)(void*, struct plume_context*, const uint8_t*, uint64_t, uint32_t))

struct plume_driver {
    /**
     * A pointer to the driver internal data.
     * 
     * Used in case the driver can be used for multiple
     *  disks at the same time with plume.
     */
    void* driver_ptr;

    /**
     * Obtain disk information.
     */
    uint8_t (*disk_information) (void* driver_ptr, struct plume_disk* result);

    /**
     * Returns whether one can write to a block.
     * 
     * @param driver_ptr A pointer to the driver.
     * @param plume_ptr A pointer to plume's internal data
     * @return 0 if it isn't ready, otherwise it is ready.
     */
    uint8_t (*write_block_ready) (void* driver_ptr, struct plume_context* context);
    /**
     * Write to a single block
     * 
     * @param driver_ptr A pointer to the driver.
     * @param plume_ptr A pointer to plume's internal data
     * @param buffer A pointer to the content to write
     * @param block_id The identifier of the starting block in the disk
     * @return 0 if it went ok, 1 if it was sent to DMA, 2 if you should retry later
     */
    uint8_t (*write_block) (void* driver_ptr, struct plume_context* context, const uint8_t* buffer, uint64_t block_id);
    /**
     * Read from a single block
     * 
     * @param driver_ptr A pointer to the driver.
     * @param plume_ptr A pointer to plume's internal data
     * @param buffer A pointer to the buffer to store data
     * @param block_id The identifier of the starting block in the disk
     */
    uint8_t (*read_block) (void* driver_ptr, struct plume_context* context, uint8_t* buffer, uint64_t block_id);

    /**
     * Write multiple contiguous blocks (optional, may be NULL).
     *
     * When non-NULL, plume_tick() batches up to PLUME_MAX_BATCH_SIZE
     * contiguous ring-buffer blocks into a single multi-block transfer.
     * Falls back to single-block write_block() when NULL.
     *
     * @param driver_ptr  A pointer to the driver internal data.
     * @param context     A pointer to plume's internal data
     * @param buffer      Pointer to the first block (contiguous in memory)
     * @param block_id    Starting block on disk
     * @param num_blocks  Number of contiguous 512-byte blocks to write
     * @return 0 if it went ok, 1 if sent to DMA, 2 if you should retry later
     */
    uint8_t (*write_blocks) (void* driver_ptr, struct plume_context* context, const uint8_t* buffer, uint64_t block_id, uint32_t num_blocks);
};

/**
 * Write to a single block in blocking mode
 * 
 * @param driver_ptr A pointer to the driver internal data.
 * @param plume_ptr A pointer to plume's internal data
 * @param buffer A pointer to the content to write
 * @param block_id The identifier of the starting block in the disk
 * @return 0 if it went ok, 1 if it was sent to DMA, 2 if you should retry later
 */
uint8_t plume_write_block_blocking (struct plume_context* context, const uint8_t* buffer, uint64_t block_id);

#endif /* PLUME_DRIVER_H */
