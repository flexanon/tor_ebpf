#include "core/or/or.h"
#include "circpad_drop_def_conn_based.h"
#include "core/or/relay.h"
#include "core/or/plugin.h"
#include "core/or/cell_st.h"


/**
 * We just want to delay the cell?
 */

uint64_t circuit_unrecognized_data_received(relay_process_edge_t *args) {
  plugin_t * plugin = (plugin_t*) get(RELAY_ARG_PLUGIN_T, 1, args);
  circuit_t *circ = (circuit_t *) get(RELAY_ARG_CIRCUIT_T, 1, args);
  circpad_connbased_dropmark_t *ctx = (circpad_connbased_dropmark_t*) get(UTIL_CONN_CTX, 1, circ);
  cell_t *cell = (cell_t *) get(RELAY_ARG_CELL_T, 1, args);
  cell_t *mycell = my_plugin_malloc(plugin, sizeof(*mycell));
  my_plugin_memcpy(mycell, cell, sizeof(*mycell));
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Plugin: Copied cell Pointer is (%p)", mycell);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "sizeof(cell_t*): %lu", sizeof(mycell));
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "fifo size: %u", ctx->cell_queue->itemsize);
  
  queue_ret_t ret = queue_push(ctx->cell_queue, &mycell);

  cell_t **pop_cell = NULL;
  void *data;
  ret  = queue_pop(ctx->cell_queue, data);
  pop_cell = (cell_t **) data;
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Plugin: Pop_cell is is (%p)", *pop_cell);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Plugin: data is (%p)", (cell_t*) *(uint64_t*)data);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Plugin: data is in fifo (%p)", (cell_t*) ctx->cell_queue->queue[0]);

  ret = queue_push(ctx->cell_queue, &mycell);

  if (ret != OK) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "queue_push returned value %d", ret);
    return -1;
  }
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Plugin: Adding a cell to the queue. It is now of size %d", ctx->cell_queue->size);
  return 0;
}
