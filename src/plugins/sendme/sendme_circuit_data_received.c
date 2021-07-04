#include "core/or/or.h"
#include "core/or/relay.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"

uint64_t sendme_circuit_data_received(relay_process_edge_t *pedge) {
  int deliver_window, domain;
  circuit_t *circ = (circuit_t *) get(RELAY_ARG_CIRCUIT_T, 1, pedge);
  crypt_path_t *layer_hint = (crypt_path_t *) get(RELAY_ARG_CRYPT_PATH_T, 1, pedge);

  if ((int) get(UTIL_CIRCUIT_IS_ORIGIN, 1, circ)) {
    deliver_window = (int) get(RELAY_LAYER_HINT_DELIVER_WINDOW, 1, layer_hint);
    set(RELAY_LAYER_HINT_DELIVER_WINDOW, 2, layer_hint, --deliver_window);
    domain = LD_APP;
  }
  else {
    deliver_window = (int) get(RELAY_CIRC_DELIVER_WINDOW, 1, circ);
    set(RELAY_CIRC_DELIVER_WINDOW, 1, circ, --deliver_window);
    domain = LD_EXIT;
  }

  log_fn_(domain, LD_PLUGIN, __FUNCTION__,
      "Circuit deliver_window now %d.", deliver_window);
  return 0;
}
