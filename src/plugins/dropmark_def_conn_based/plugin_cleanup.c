#include "circpad_drop_def_conn_based.h"
#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/plugin_memory.h"
#include "util/container.c"


uint64_t plugin_cleanup(plugin_plugin_args_t *args) {
  plugin_t *plugin = (plugin_t *) get(PLUGIN_ARG_PLUGIN_T, 1, args);
  circuit_t *circ = (circuit_t *) get(PLUGIN_ARG_CIRCUIT_T, 1, args);
  circpad_connbased_dropmark_t* ctx = (circpad_connbased_dropmark_t*) get(UTIL_CONN_CTX, 1, circ);

  fifo_free(plugin, ctx->cell_queue);
  my_plugin_free(ctx);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "Freed the conn plugin ctx");
  return 0;
}
