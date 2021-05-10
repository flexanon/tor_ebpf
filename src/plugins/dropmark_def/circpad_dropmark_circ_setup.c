#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/connection_edge.h"
#include "core/or/circuitpadding.h"

/**
 * this code accesses the padding_info of a just set up circuit, and
 * use it to link a unique name of the padding machine such that this plugin can
 * refer to it later on
 */

uint64_t circpad_dropmark_circ_setup(conn_edge_plugin_args_t *args) {
  circpad_machine_spec_t *machine = (circpad_machine_spec_t*) get(CIRCPAD_MACHINE_SPEC_T, 1, args);
  circpad_machine_runtime_t *mr = (circpad_machine_runtime_t*) get(CIRCPAD_ARG_MACHINE_RUNTIME, 1, args);
  if (!strcmp(machine->name, "client_dropmark_def") || !strcmp(machine->name, "relay_dropmark_def")) {
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Setting machine name:%s in PLUGIN_MACHINE_RUNTIME void ptr", machine->name);
    set(CIRCPAD_PLUGIN_MACHINE_RUNTIME, 2, mr, (char *) machine->name);
  }
  return 0;
}
