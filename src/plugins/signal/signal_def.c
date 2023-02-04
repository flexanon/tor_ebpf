#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "plugins/signal/signal_def.h"
#include "core/or/circuitpadding.h"
#include "ubpf/vm/plugin_memory.h"

static int num_events = CIRCPAD_NUM_EVENTS + PLUGIN_NUM_EVENTS;

uint64_t circpad_dropmark_defense(circpad_plugin_args_t *args) {
  //register events
  plugin_t *plugin = (plugin_t *) get(CIRCPAD_ARG_PLUGIN_T, 1, args);
  circpad_dropmark_t *ctx = my_plugin_malloc(plugin, sizeof(*ctx));
  ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  ctx->CIRCPAD_EVENT_SIGPLUGIN_CLOSE = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  set(CIRCPAD_PLUGIN_CTX, 2, args, ctx);
  return 0;
}
