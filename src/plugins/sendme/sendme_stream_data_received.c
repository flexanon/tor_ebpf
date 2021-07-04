#include "core/or/or.h"
#include "core/or/relay.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"

uint64_t sendme_stream_data_received(relay_process_edge_t *pedge) {
  int conn_del_window = (int) get(RELAY_CONN_DELIVER_WINDOW, 1, pedge);
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "conn_del_window %d", conn_del_window);
  set(RELAY_CONN_DELIVER_WINDOW, 2, pedge, --conn_del_window);
  if (conn_del_window < 0)
    return -1;
  else
    return 0;
}
