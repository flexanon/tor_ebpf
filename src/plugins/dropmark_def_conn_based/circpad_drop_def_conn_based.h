#ifndef TOR_CONNBASED_DROPMARDEF_H
#define TOR_CONNBASED_DROPMARDEF_H

#include "util/container.c"

typedef struct circpad_connbased_dropmark_t {
  fifo_t *cell_queue;
} circpad_connbased_dropmark_t;


#endif
