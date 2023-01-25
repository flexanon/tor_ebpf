#include "ubpf/vm/plugin_memory.h"
#include <stdlib.h>

/** Ring buffer to keep ordered FIFO data in plugins when needed */


typedef enum queue_ret {
  OK,
  MEMORY_FULL,
  EMPTY
} queue_ret_t;

typedef struct st_fifo_t {
  int max_num;
  int size;
  int itemsize;
  uint8_t *queue;
  uint8_t *front;
  uint8_t *back;
  int front_idx;
  int back_idx;
} fifo_t;


static __attribute__((always_inline)) fifo_t *
queue_new(plugin_t *plugin, int max_num, int itemsize) {
  fifo_t *fifo = my_plugin_malloc(plugin, sizeof(*fifo));
  my_plugin_memset(fifo, 0, sizeof(*fifo));
  if (fifo == NULL)
    return NULL;
  fifo->queue = my_plugin_malloc(plugin, max_num*itemsize);
  my_plugin_memset(fifo->queue, 0, max_num*itemsize);
  if (fifo->queue == NULL) {
    my_plugin_free(plugin, fifo);
    return NULL;
  }
  fifo->size = 0;
  fifo->front_idx = 0;
  fifo->back_idx = 0;
  fifo->max_num = max_num;
  fifo->itemsize = itemsize;
  return fifo;
}

/**
 * Push data the front of the queue
 *
 * return MEMORY_FULL, OK
 */
static __attribute__((always_inline)) queue_ret_t
tcpls_record_queue_push(fifo_t *fifo, void *data) { 
  if (fifo->size == fifo->max_num)
    return MEMORY_FULL;
  my_plugin_memcpy(&fifo->queue[fifo->front_idx], data, fifo->itemsize);
  fifo->size++;
  if (fifo->front_idx == (fifo->max_num-1)*fifo->itemsize) {
    fifo->front_idx = 0;
  }
  else {
    fifo->front_idx += fifo->itemsize;
  }
  return OK;
}

static __attribute__((always_inline)) queue_ret_t
queue_del(fifo_t *fifo, int n) {
  while (n > 0) {
    if (fifo->size == 0)
      return EMPTY;
    if (fifo->back_idx == (fifo->max_num - 1)*fifo->itemsize) {
      fifo->back_idx = 0;
    }
    else {
      fifo->back_idx+= fifo->itemsize;
    }
    fifo->size--;
    n--;
  }
  return OK;
}

/** Never pop-then-push.
 *
 * Intended behaviour is pop-then-consume (either memcpy or use and
 * drop reference). Then one can add things to the queue.
 *
 * Would lead to unexpected behavior otherwise.
 */

static __attribute__((always_inline)) queue_ret_t
queue_pop(fifo_t *fifo, void *data) {
  if (fifo->size == 0)
    return EMPTY;
  data = &fifo->queue[fifo->back_idx];
  return queue_del(fifo, 1);
}

static __attribute__((always_inline)) void
fifo_free(plugin_t *plugin, fifo_t *fifo) {
  if (!fifo)
    return;
  if (!fifo->queue) {
    my_plugin_free(plugin, fifo);
    return;
  }
  my_plugin_free(plugin, fifo->queue);
  my_plugin_free(plugin, fifo);
}
