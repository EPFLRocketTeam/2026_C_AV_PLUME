
#ifndef PLUME_WRITER_H
#define PLUME_WRITER_H

#include "plume/context.h"

uint8_t plume_open_write (struct plume_context* context);
uint8_t plume_write (struct plume_context* context, const uint8_t* buffer, uint64_t size);
uint8_t plume_tick (struct plume_context* context);

#endif /* PLUME_WRITER_H */
