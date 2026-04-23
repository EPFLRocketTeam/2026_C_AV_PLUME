
#ifndef PLUME_STATUS_H
#define PLUME_STATUS_H

#include <stdint.h>

#define PLUME_OK ((uint8_t) 0)
#define PLUME_OK_SENT_DMA ((uint8_t) 1)
#define PLUME_OK_RETRY ((uint8_t) 2)

#define PLUME_EBAD_DRIVER ((uint8_t) -1)
#define PLUME_EOOB ((uint8_t) -2)
#define PLUME_ENULL ((uint8_t) -3)
#define PLUME_EARENA_TOO_SMALL ((uint8_t) -4)
#define PLUME_EBAD_DISK ((uint8_t) -5)
#define PLUME_EMOCKED ((uint8_t) -6)
#define PLUME_EFAT_FULL ((uint8_t) -7)
#define PLUME_EDISK_FULL ((uint8_t) -8)
#define PLUME_EWRONG_PAGE_KIND ((uint8_t) -9)
#define PLUME_EWRONG_CRC ((uint8_t) -10)

uint8_t plume_is_ok (uint8_t status);

#endif /* PLUME_STATUS_H */
