#include "core/or/or.h"
#include "core/or/relay.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"

uint64_t consider_sending_sendme(relay_process_edge_t *pedge) {
  int layer_hint_del_window = 0;
  int circ_del_window = 0;
  if (pedge->layer_hint)
    layer_hint_del_window = (int) get(RELAY_LAYER_HINT_DELIVER_WINDOW, pedge->layer_hint);
  else
    circ_del_window = (int) get(RELAY_CIRC_DELIVER_WINDOW, pedge->circ);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "layer_hint del is %d", layer_hint_del_window);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "circ_del_window is %d", circ_del_window);
  
  while ((pedge->layer_hint ? layer_hint_del_window : circ_del_window) <=
      CIRCWINDOW_START -CIRCWINDOW_INCREMENT) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Queuing circuit sendme in the plugin :)");
    if (layer_hint_del_window) {
      layer_hint_del_window += CIRCWINDOW_INCREMENT;
      set(RELAY_LAYER_HINT_DELIVER_WINDOW, pedge->layer_hint, layer_hint_del_window);
    }
    else {
      circ_del_window += CIRCWINDOW_INCREMENT;
      set(RELAY_CIRC_DELIVER_WINDOW, pedge->circ, circ_del_window);
    }

    if (call_host_func(RELAY_SEND_COMMAND_FROM_EDGE, pedge) < 0) {
      log_fn_(LOG_WARN, LD_PLUGIN, __FUNCTION__,
          "relay_send_command_from_edge_ failed in plugin");
      return 1;
    }
  }
  return 0;
}
