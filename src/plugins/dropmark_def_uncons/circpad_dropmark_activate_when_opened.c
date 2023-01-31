#include "core/or/or.h"
#include "core/or/plugin.h"
#include "core/or/connection_edge.h"
#include "core/or/circuitpadding.h"
#include "plugins/dropmark_def_uncons/circpad_dropmark_def.h"


uint64_t circpad_dropmark_activate_when_built(circpad_plugin_args_t *args) {
  
  /** Check whether we're a client. If yes, check whehtehr client_dropmark_def
   * exists and send a signal to activate it on the relay if yes */
  circuit_t *circ = (circuit_t *) get(CIRCPAD_ARG_CIRCUIT_T, 1, args);
  int machine_ctr = (int) get(CIRCPAD_MACHINE_CTR, 3, circ, "client_dropmark_def", (int) 19);
  if (machine_ctr > CIRCPAD_MAX_MACHINES) {
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Looks like the client_dropmark_def machine does not exist over this circuit");
    return PLUGIN_RUN_DEFAULT;
  }
  /* is this instance a relay?*/
  int is_relay = (int) get(UTIL_IS_RELAY, 0);
  if (is_relay) {
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Looks like this circuit is built by a relay -- not sending any activate signal");
    return PLUGIN_RUN_DEFAULT;
  }


  /** Ok, we're a client, and the client_dropmark_def machine exists over this
   * circuit, so let's send a signal, shall we? */

  
  entry_point_map_t pmap;
  memset(&pmap, 0, sizeof(pmap));
  pmap.ptype = PLUGIN_DEV;
  // we're looking for the plugin that is expected to add something at the  end of the function
  pmap.putype = PLUGIN_CODE_ADD;
  // this is a edge conn protocol plugin
  pmap.pfamily = PLUGIN_PROTOCOL_CONN_EDGE;
  /** this is the same code than the one used when we send a begin cell */
  pmap.entry_name = (char *)"circpad_dropmark_send_sig";
  //XXX fixme
  pmap.param = CIRCPAD_EVENT_SHOULD_SIGPLUGIN_ACTIVATE;
  caller_id_t caller = CONNECTION_EDGE_ADD_TO_SENDING_BEGIN;
  conn_edge_plugin_args_t cargs;
  memset(&cargs, 0, sizeof(cargs));
  cargs.param = CIRCPAD_EVENT_SHOULD_SIGPLUGIN_ACTIVATE;
  cargs.on_circ = circ;
  invoke_plugin_operation_or_default(&pmap, caller, (void*)&cargs);
  return 0;
}
