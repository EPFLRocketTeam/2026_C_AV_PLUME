
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include "plume/status.h"
#include "plume/drivers/unix.h"

uint8_t unix_goto_block (struct plume_unix_driver* driver_ptr, uint64_t block_id) {
    if (driver_ptr->current_block == block_id) {
        return PLUME_OK;
    }

    ssize_t status = lseek(driver_ptr->fd, block_id * driver_ptr->block_size, SEEK_SET);
    if (status < 0) {
        return PLUME_EBAD_UNIX_LSEEK;
    }

    driver_ptr->current_block = block_id;
    return PLUME_OK;
}

uint8_t unix_disk_information (struct plume_unix_driver* driver_ptr, struct plume_disk* result) {
    result->number_blocks = driver_ptr->block_count;
    result->block_size    = driver_ptr->block_size;
    
    return PLUME_OK;
}

uint8_t unix_write_block_ready (
    struct plume_unix_driver* driver_ptr,
    struct plume_context* context
) {
    return 1;
}
uint8_t unix_write_block (
    struct plume_unix_driver* driver_ptr,
    struct plume_context* context,
    const uint8_t* buffer, 
    uint64_t block_id
) {
    ssize_t status = unix_goto_block(driver_ptr, block_id);
    if (status != PLUME_OK) {
        return status;
    }

    status = write(driver_ptr->fd, buffer, driver_ptr->block_size);
    status = status != driver_ptr->block_size ? PLUME_EBAD_UNIX_RW : PLUME_OK;

    if (status == PLUME_OK) {
        driver_ptr->current_block ++;
    } else {
        driver_ptr->current_block = -1;
    }

    return status;
}
uint8_t unix_write_blocks (
    struct plume_unix_driver* driver_ptr,
    struct plume_context* context,
    const uint8_t* buffer, 
    uint64_t block_id,
    uint32_t count
) {
    ssize_t status = unix_goto_block(driver_ptr, block_id);
    if (status != PLUME_OK) {
        return status;
    }

    status = write(driver_ptr->fd, buffer, driver_ptr->block_size * count);
    status = status != driver_ptr->block_size ? PLUME_EBAD_UNIX_RW : PLUME_OK;

    if (status == PLUME_OK) {
        driver_ptr->current_block += count;
    } else {
        driver_ptr->current_block = -1;
    }

    return status;
}
uint8_t unix_read_block  (
    struct plume_unix_driver* driver_ptr,
    struct plume_context* context,
    uint8_t* buffer, 
    uint64_t block_id
) {
    ssize_t status = unix_goto_block(driver_ptr, block_id);
    if (status != PLUME_OK) {
        return status;
    }

    status = read(driver_ptr->fd, buffer, driver_ptr->block_size);
    status = status != driver_ptr->block_size ? PLUME_EBAD_UNIX_RW : PLUME_OK;

    if (status == PLUME_OK) {
        driver_ptr->current_block ++;
    } else {
        driver_ptr->current_block = -1;
    }

    return status;
}


int plume_allocate_unix_driver (struct plume_driver* out, const char* device) {
    struct plume_unix_driver* drv = (struct plume_unix_driver*) malloc(sizeof(struct plume_unix_driver));
    
    drv->fd = open(device, O_RDWR | O_SYNC);
    if (drv->fd < 0) {
        free(drv);
        return PLUME_EBAD_UNIX_OPEN;
    }

    uint64_t size_bytes = 0;
    if (ioctl(drv->fd, BLKGETSIZE64, &size_bytes) < 0) {
        close(drv->fd);
        free(drv);
        return PLUME_EBAD_UNIX_DISKLEN;
    }

    uint64_t block_size = 512;
    ioctl(drv->fd, BLKSSZGET, &block_size);

    drv->block_size    = block_size;
    drv->block_count   = size_bytes / block_size;
    drv->current_block = 0;

    out->driver_ptr = drv;

    out->disk_information  = PLUME_DISK_INFORMATION_FN_TYPE  &unix_disk_information;
    out->read_block        = PLUME_READ_BLOCK_FN_TYPE        &unix_read_block;
    out->write_block       = PLUME_WRITE_BLOCK_FN_TYPE       &unix_write_block;
    out->write_block_ready = PLUME_WRITE_BLOCK_READY_FN_TYPE &unix_write_block_ready;
    out->write_blocks      = PLUME_WRITE_BLOCKS_FN_TYPE      &unix_write_blocks;

    return 0;
}
void plume_free_unix_driver (struct plume_driver* driver) {
    struct plume_unix_driver* drv = (struct plume_unix_driver*) driver->driver_ptr;
    
    if (drv) {
        if (drv->fd >= 0) {
            close(drv->fd);
        }

        free(driver->driver_ptr);
        driver->driver_ptr = NULL;
    }
}