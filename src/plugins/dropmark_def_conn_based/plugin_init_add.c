#include "circpad_drop_def_conn_based.h"
#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/plugin_memory.h"

uint64_t plugin_init_fn(plugin_plugin_args_t *args) {

  plugin_t *plugin = (plugin_t *) get(PLUGIN_ARG_PLUGIN_T, 1, args);
  circuit_t *circ = (circuit_t *) get(PLUGIN_ARG_CIRCUIT_T, 1, args);
  /** create the plugin ctx */
  circpad_connbased_dropmark_t *ctx = my_plugin_malloc(plugin,
      sizeof(*ctx));
  if (!ctx) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
        "Unable to malloc circpad_connbased_dropmark_t");
    return -1;
  }
  ctx->cell_queue = queue_new(plugin, 64);
  if (!ctx->cell_queue) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
        "Unable to malloc cell_queue");
    return -1;
  }
  set(UTIL_CONN_CTX, 2, circ, (void *)ctx);
  return 0;
}

