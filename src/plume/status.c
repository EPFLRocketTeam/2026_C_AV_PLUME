
#include "plume/status.h"

uint8_t plume_is_ok (uint8_t status) {
    return status == PLUME_OK || status == PLUME_OK_SENT_DMA;
}