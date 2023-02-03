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
  cell_direction_t direction = (cell_direction_t) get(RELAY_ARG_CELL_DIRECTION_T, 1, args);
  if (direction == CELL_DIRECTION_OUT) {
    /** we're only interested in delaying inward cells */
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Caught a cell with direction out. Ignoring"); 
    return PLUGIN_RUN_DEFAULT;
  }
  circpad_connbased_dropmark_t *ctx = (circpad_connbased_dropmark_t*) get(UTIL_CONN_CTX, 1, circ);
  ctx->ctr_seen_cell++;

  cell_t *cell = (cell_t *) get(RELAY_ARG_CELL_T, 1, args);
  cell_t *mycell = my_plugin_malloc(plugin, sizeof(*mycell));
  my_plugin_memcpy(mycell, cell, sizeof(*mycell));
  queue_ret_t ret = queue_push(ctx->cell_queue, &mycell);

  if (ret != OK) {
    my_plugin_free(plugin, mycell);
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "queue_push returned value %d -- Maybe we should raise the size", ret);
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "We have seen %d cells; cleaning up", ctx->ctr_seen_cell);
    uint64_t plugin_to_cleanup = 42;
    caller_id_t caller = PLUGIN_HOUSEKEEPING_CLEANUP_CALLED;
    entry_point_map_t pmap;
    my_plugin_memset(&pmap, 0, sizeof(pmap));
    pmap.entry_name = (char *) "plugin_cleanup";
    plugin_plugin_args_t cargs;
    my_plugin_memset(&cargs, 0, sizeof(cargs));
    cargs.plugin = plugin;
    cargs.circ = circ;
    invoke_plugin_operation_or_default(&pmap, caller, (void*) &cargs);
    return PLUGIN_ERROR;
  }
  return 0;
}
