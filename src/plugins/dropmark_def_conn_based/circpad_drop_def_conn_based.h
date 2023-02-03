#ifndef TOR_CONNBASED_DROPMARDEF_H
#define TOR_CONNBASED_DROPMARDEF_H

#include "util/container.c"

typedef struct circpad_connbased_dropmark_t {
  fifo_t *cell_queue;
  uint32_t ctr_seen_cell;
} circpad_connbased_dropmark_t;


#endif
