
import os
import sys
import zlib
import struct
import argparse

from typing import List

PACKET_HEADER_SIZE = 8

PLUME_PAGE_SETTINGS  = 0b11001100
PLUME_PAGE_FILESTART = 0b10101010
PLUME_PAGE_FILECONT  = 0b01010101
PLUME_PAGE_FILEINFO  = 0b00110011

def create_page_settings (bss: int, fatsize: int):
    return struct.pack("<BQ", PLUME_PAGE_SETTINGS, fatsize) + bytes([0] * (bss - 9))
def create_page (content: bytes, is_first: bool):
    header = struct.pack(
        "B3xI",
        (PLUME_PAGE_FILESTART if is_first else PLUME_PAGE_FILECONT),
        zlib.crc32(content) & 0xFFFFFFFF
    )

    return header + content
def create_empty_page (bss: int):
    return bytes([0] * bss)
def create_fileinfo_page (bss: int, pointer: int):
    return struct.pack("<BQ", PLUME_PAGE_FILEINFO, pointer) + bytes([0] * (bss - 9))

class DiskFile:
    def __init__ (self, pages: List[bytes]):
        self.pages = pages

    @staticmethod
    def from_file (bss: int, file: str):
        with open(file, "rb") as fr:
            content = fr.read()
        
        data_sector_size = bss - PACKET_HEADER_SIZE
        pages = [
            create_page(content[i:i + data_sector_size], i == 0)
            for i in range(0, len(content), data_sector_size)
            if i + data_sector_size <= len(content)
        ]

        return DiskFile(pages)

def main ():
    parser = argparse.ArgumentParser("Plume-DiskImageGenerator")
    parser.add_argument("fatsize", type=int)
    parser.add_argument("bss", type=int)
    parser.add_argument("sze", type=int)
    parser.add_argument("folder")

    args = parser.parse_args()

    files = os.listdir(args.folder)

    pages: List[List[bytes]] = []
    for file_id in range(len(files)):
        if str(file_id) not in files:
            raise ValueError(f"ERROR: Missing file {file_id}")
        pages.append(DiskFile.from_file( args.bss, os.path.join(args.folder, str(file_id)) ).pages)

    header = [create_page_settings(args.bss, args.fatsize)]
    total = 0
    for i in range(len(pages)):
        header.append(create_fileinfo_page(args.bss, total + args.fatsize))
        total += len(pages[i])
    while len(header) < args.fatsize:
        header.append(create_empty_page(args.bss))
    for pg in pages:
        header.extend(pg)
    while len(header) < args.sze:
        header.append(create_empty_page(args.bss))
    for idx, page in enumerate(header):
        assert len(page) == args.bss, f"invalid page {idx}, {len(page)}"
        sys.stdout.buffer.write(page)

if __name__ == "__main__":
    main()
