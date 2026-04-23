
#include "plume/header.h"
#include "plume/const.h"
#include "plume/status.h"

static uint32_t plume_crc32_table[256];
static int      plume_crc32_table_ready = 0;

static void plume_crc32_build_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320; // reflected polynomial
            else
                crc >>= 1;
        }
        plume_crc32_table[i] = crc;
    }
    plume_crc32_table_ready = 1;
}

uint32_t plume_crc32(const void *data, uint64_t length)
{
    if (!plume_crc32_table_ready)
        plume_crc32_build_table();

    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF; // initial value

    while (length--)
        crc = (crc >> 8) ^ plume_crc32_table[(crc ^ *buf++) & 0xFF];

    return crc ^ 0xFFFFFFFF;  // final XOR
}

void plume_prepare_block (uint8_t* block, uint64_t block_size, uint8_t is_first_block) {
    struct plume_header* header = (struct plume_header*) block;

    uint8_t block_kind = is_first_block ? PLUME_PAGE_FILESTART : PLUME_PAGE_FILECONT;
    header->page_kind = block_kind;
    header->crc = plume_crc32(block + sizeof(struct plume_header), block_size - sizeof(struct plume_header));
}
uint8_t plume_verify_block  (uint8_t* block, uint64_t block_size, uint8_t is_first_block) {
    struct plume_header* header = (struct plume_header*) block;

    uint8_t block_kind = is_first_block ? PLUME_PAGE_FILESTART : PLUME_PAGE_FILECONT;
    if (header->page_kind != block_kind)
        return PLUME_EWRONG_PAGE_KIND;

    uint32_t expected_crc = plume_crc32(block + sizeof(struct plume_header), block_size - sizeof(struct plume_header));
    if (header->crc != expected_crc)
        return PLUME_EWRONG_CRC;
    
    return PLUME_OK;
}
