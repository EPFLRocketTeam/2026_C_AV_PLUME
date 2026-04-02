
#ifndef PLUME_WRITER_H
#define PLUME_WRITER_H

#include "plume/context.h"

void plume_open_write (struct plume_context* context);
void plume_write (struct plume_context* context, uint8_t* buffer, uint64_t size);
void plume_flush (struct plume_context* context);

#endif /* PLUME_WRITER_H */
