#include "circpad_drop_def_conn_based.h"
#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/plugin_memory.h"


uint64_t plugin_cleanup(plugin_plugin_args_t *args) {
  plugin_t *plugin = (plugin_t *) get(PLUGIN_ARG_PLUGIN_T, 1, args);
  circuit_t *circ = (circuit_t *) get(PLUGIN_ARG_CIRCUIT_T, 1, args);
  circpad_connbased_dropmark_t* ctx = (circpad_connbased_dropmark_t*) get(UTIL_CONN_CTX, 1, circ);

  cell_t *cell = NULL;
  queue_ret_t ret;
  if (ctx && ctx->cell_queue) {
    channel_t *chan = (channel_t *) get(RELAY_CIRCUIT_CHAN_T, 2, circ, CELL_DIRECTION_IN);
    while ((ret = queue_pop(ctx->cell_queue, &cell)) != EMPTY) {
      /** flush the cell then free it */
      // Do we signal a state event for the machine padding?*/
      call_host_func(RELAY_APPEND_CELL_TO_CIRCUIT_QUEUE, 3, circ, chan, cell);
      my_plugin_free(plugin, cell);
    }
    fifo_free(plugin, ctx->cell_queue);
    my_plugin_free(plugin, ctx);
    set(UTIL_CONN_CTX, 2, circ, NULL);
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "Freed the conn plugin ctx");
  }
  return 0;
}
