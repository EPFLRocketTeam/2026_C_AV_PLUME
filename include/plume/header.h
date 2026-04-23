
#ifndef PLUME_HEADER_H
#define PLUME_HEADER_H

#include <assert.h>
#include <stdint.h>

#pragma pack(push, 1)
struct plume_header {
    uint8_t  page_kind;
    uint8_t  blank [3];
    uint32_t crc;
};
#pragma pack(pop)

static_assert(sizeof(struct plume_header) == 8, "Header size mismatch !");

uint32_t plume_crc32(const void *data, uint64_t length);

void    plume_prepare_block (uint8_t* block, uint64_t block_size, uint8_t is_first_block);
uint8_t plume_verify_block  (uint8_t* block, uint64_t block_size, uint8_t is_first_block);

#endif
