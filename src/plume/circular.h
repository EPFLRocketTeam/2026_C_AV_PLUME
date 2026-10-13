
#ifndef PLUME_CIRCULAR_H
#define PLUME_CIRCULAR_H

/* On disk layout of the circular mode. Private to the library and the CLI.
 *
 * Settings page: byte 9 holds the mode flags, 0 on every disk cleared before
 * this mode existed. Data pages: the 3 blank header bytes hold the lap in
 * which the page was written, little endian. FILEINFO pages: bytes 9 to 11
 * hold the lap of the file's first page. Laps start at 1. */

#include <stdint.h>

#define PLUME_SETTINGS_FLAGS_OFFSET 9
#define PLUME_SETTINGS_CIRCULAR     0x01

#define PLUME_HEADER_LAP_OFFSET     1
#define PLUME_FILEINFO_LAP_OFFSET   9

#define PLUME_LAP_MASK 0xFFFFFFu

static inline uint32_t plume_lap_read (const uint8_t* bytes) {
    return (uint32_t) bytes[0] | ((uint32_t) bytes[1] << 8) | ((uint32_t) bytes[2] << 16);
}
static inline void plume_lap_write (uint8_t* bytes, uint32_t lap) {
    bytes[0] = (uint8_t) lap;
    bytes[1] = (uint8_t) (lap >> 8);
    bytes[2] = (uint8_t) (lap >> 16);
}
static inline uint32_t plume_lap_next (uint32_t lap) {
    lap = (lap + 1) & PLUME_LAP_MASK;
    return lap == 0 ? 1 : lap;
}

#endif /* PLUME_CIRCULAR_H */
