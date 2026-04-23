
#include <gtest/gtest.h>

#include <malloc.h>

extern "C" {
    #include "plume/header.h"
    #include "plume/status.h"
    #include "plume/const.h"
}
TEST(HeaderTests, TestCRC32) {
    uint8_t buffer[ sizeof(struct plume_header) + 5 ];
    memcpy(buffer + sizeof(struct plume_header), "hello", 5);
    EXPECT_EQ(plume_crc32(buffer + sizeof(struct plume_header), 5), 0x3610a686);
    // Should be reliable accross multiple calls to the same value.
    EXPECT_EQ(plume_crc32(buffer + sizeof(struct plume_header), 5), 0x3610a686);
}
TEST(HeaderTests, TestPrepareFirstHelloBlock) {
    uint8_t buffer[ sizeof(struct plume_header) + 5 ];
    memcpy(buffer + sizeof(struct plume_header), "hello", 5);

    plume_prepare_block(buffer, sizeof(struct plume_header) + 5, 1);
    struct plume_header* header = (struct plume_header*) buffer;
    EXPECT_EQ(header->page_kind, PLUME_PAGE_FILESTART);
    EXPECT_EQ(header->crc, 0x3610a686);
}
TEST(HeaderTests, TestPrepareSecondHelloBlock) {
    uint8_t buffer[ sizeof(struct plume_header) + 5 ];
    memcpy(buffer + sizeof(struct plume_header), "hello", 5);

    plume_prepare_block(buffer, sizeof(struct plume_header) + 5, 0);
    struct plume_header* header = (struct plume_header*) buffer;
    EXPECT_EQ(header->page_kind, PLUME_PAGE_FILECONT);
    EXPECT_EQ(header->crc, 0x3610a686);
}
TEST(HeaderTests, TestVerifyFirstHelloBlock) {
    uint8_t buffer[ sizeof(struct plume_header) + 5 ];
    memcpy(buffer + sizeof(struct plume_header), "hello", 5);
    struct plume_header* header = (struct plume_header*) buffer;
    header->page_kind = PLUME_PAGE_FILESTART;
    header->crc = 0x3610a686;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 1), PLUME_OK);
    header->page_kind = PLUME_PAGE_FILECONT;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 1), PLUME_EWRONG_PAGE_KIND);
    header->page_kind = PLUME_PAGE_EMPTY0;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 1), PLUME_EWRONG_PAGE_KIND);
    header->page_kind = PLUME_PAGE_FILESTART;
    header->crc = 0x3610b686;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 1), PLUME_EWRONG_CRC);
}
TEST(HeaderTests, TestVerifySecondHelloBlock) {
    uint8_t buffer[ sizeof(struct plume_header) + 5 ];
    memcpy(buffer + sizeof(struct plume_header), "hello", 5);
    struct plume_header* header = (struct plume_header*) buffer;
    header->page_kind = PLUME_PAGE_FILECONT;
    header->crc = 0x3610a686;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 0), PLUME_OK);
    header->page_kind = PLUME_PAGE_FILESTART;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 0), PLUME_EWRONG_PAGE_KIND);
    header->page_kind = PLUME_PAGE_EMPTY0;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 0), PLUME_EWRONG_PAGE_KIND);
    header->page_kind = PLUME_PAGE_FILECONT;
    header->crc = 0x3610b686;
    EXPECT_EQ(plume_verify_block(buffer, sizeof(struct plume_header) + 5, 0), PLUME_EWRONG_CRC);
}
