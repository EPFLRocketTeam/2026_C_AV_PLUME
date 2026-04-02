
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

uint8_t plume_is_ok (uint8_t status);

#endif /* PLUME_STATUS_H */
