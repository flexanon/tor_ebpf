/*
 * \file plugin.c
 * \brief Handle plugin main operations, implement the API definition to
 * interact with plugins
 **/
#define PLUGIN_PRIVATE
#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/circuitpadding.h"
#include "core/or/connection_edge.h"
#include "core/or/circuit_st.h"
#include "core/or/channel.h"
#include "core/or/circuitlist.h"
#include "core/or/crypt_path.h"
#include "core/or/origin_circuit_st.h"
#include "core/or/or_circuit_st.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "core/or/relay.h"
#include "core/or/signal_attack.h"
#include "core/or/cell_st.h"
#include "feature/relay/routermode.h"
#include "ubpf/vm/inc/ubpf.h"
#include "trunnel/plug_cell.h"
#include <time.h>

static int plugin_is_caller_id_system_wide(caller_id_t caller);
/**
 * Hashtable containing plugin information 
 */
static inline int
plugin_entries_eq_(entry_point_map_t *a, entry_point_map_t *b) {
  return !strcmp(a->entry_name, b->entry_name) && a->param == b->param &&
         a->ptype == b->ptype && a->putype == b->putype &&
         a->pfamily == b->pfamily;
}

static inline unsigned int
plugin_entry_hash_(entry_point_map_t *a) {

  uint32_t array[5+256/4] = {0,}; /** putype+ptype+ max name authorized*/
  array[0] = a->ptype;
  array[1] = a->putype;
  array[2] = a->pfamily;
  memcpy(&array[5], a->entry_name, strlen(a->entry_name));
  return (unsigned) siphash24g(array, sizeof(array));
}

static HT_HEAD(plugin_map_ht, entry_point_map_t)
    plugin_map_ht = HT_INITIALIZER();

HT_PROTOTYPE(plugin_map_ht, entry_point_map_t, node,
             plugin_entry_hash_, plugin_entries_eq_);
HT_GENERATE2(plugin_map_ht, entry_point_map_t, node,
             plugin_entry_hash_, plugin_entries_eq_, 0.6,
             tor_reallocarray_, tor_free_);

STATIC int plugins_compare_by_uid_(const void **a_, const void **b_) {
  const plugin_t *a = *a_;
  const plugin_t *b = *b_;
  uint64_t uid_a = a->uid;
  uint64_t uid_b = b->uid;
  if (uid_a < uid_b)
    return -1;
  else if (uid_a == uid_b)
    return 0;
  else
    return 1;
}

/****************************************************************/

int plugin_plug_elf(plugin_t *plugin, entry_info_t *einfo, char *elfpath) {
  entry_point_map_t *found = NULL;
  if (plugin->is_system_wide) {
    /** Check whether we already loaded it */
    entry_point_map_t search;
    search.entry_name = tor_strdup(einfo->entry_name);
    search.ptype = einfo->ptype;
    search.putype = einfo->putype;
    search.pfamily = einfo->pfamily;
    search.param = einfo->param;
    found = HT_FIND(plugin_map_ht, &plugin_map_ht, &search);
    if (found) {
      /** XXX What shoud we do?*/
      tor_free(einfo->entry_name);
      log_debug(LD_PLUGIN, "A plugin with these characteristics:\
          name: %s\
          ptype: %d\
          putype: %d\
          pfamily: %d\
          param: %d\
          is already part of our map", search.entry_name, search.ptype, search.putype,
          search.pfamily, search.param);
      return 0;
    }
    tor_free(search.entry_name);
  }
  plugin_entry_point_t *entry_point = tor_malloc_zero(sizeof(*entry_point));
  int ret;
  /*clock_t start, end;*/
  /*double cpu_time_used;*/
  /*start = clock();*/
  ret = load_elf_file(elfpath,  plugin, entry_point);
  /*end = clock();*/
  /*cpu_time_used =  ((double) (end - start)) / CLOCKS_PER_SEC;*/
  /*log_info(LD_PLUGIN, "Loading Plugin entry_point %s took %f sec", search.entry_name, cpu_time_used);*/
  if (ret < 0) {
    log_debug(LD_PLUGIN, "Failed to load plugin at elfpath %s, with heap of size %lu bytes", elfpath,
        plugin->memory_size);
    tor_free(entry_point);
    return -1;
  }
  entry_point->entry_name = tor_strdup(einfo->entry_name);
  entry_point->plugin = plugin;
  /*tor_free(einfo->entry_name);*/
  smartlist_add(plugin->entry_points, entry_point);
  if (plugin->is_system_wide) {
    found = tor_malloc_zero(sizeof(entry_point_map_t));
    found->plugin = plugin;
    found->entry_point = entry_point;
    /*take ownership */
    found->entry_name = tor_strdup(einfo->entry_name);
    found->putype = einfo->putype;
    found->pfamily = einfo->pfamily;
    found->ptype = einfo->ptype;
    /** Register the plugin; do it for each family*/
    found->param = einfo->param;
    memcpy(&entry_point->info, einfo, sizeof(entry_point->info));
    HT_INSERT(plugin_map_ht, &plugin_map_ht, found);
    log_debug(LD_PLUGIN, "Inserted plugin name:%s; putype:%d, ptype: %d,\
          pfamily:%d, param:%d in map", found->entry_name, found->putype,
          found->ptype, found->pfamily, found->param);
    /*tor_free(found);*/
  }
  else {
    tor_free(einfo->entry_name);
  }
  return 0;
}

/**
 * Given parameters, look into loaded plugins if one
 * matches. If not, call default code. 
 */

int invoke_plugin_operation_or_default(entry_point_map_t *key,
    caller_id_t caller, void *args) {
  if (!get_options()->EnablePlugins) {
    log_debug(LD_PLUGIN, "Plugins not enabled; defaulting to existing code");
    return PLUGIN_RUN_DEFAULT;
  }
  entry_point_map_t *found = NULL;
  int is_system_wide = plugin_is_caller_id_system_wide(caller);
  if (key && is_system_wide) {
    found = HT_FIND(plugin_map_ht, &plugin_map_ht, key);
    if (found)
      log_debug(LD_PLUGIN, "Plugin found for caller %s",
                plugin_caller_id_to_string(caller));
  }
  if (found || !is_system_wide) {
    switch (caller) {
      case RELAY_CIRCUIT_UNRECOGNIZED_DATA_RECEIVED:
        {
          struct relay_process_edge_t *ctx = (relay_process_edge_t *) args;
          // XXX ep should ideally have a pointer to its plugin
          /*plugin_t *plugin = circuit_plugin_get(ctx->circ, 42);*/
          plugin_entry_point_t *ep = circuit_plugin_entry_point_get(ctx->circ, key->entry_name);
          if (!ep) {
            log_debug(LD_PLUGIN, "No conn plugin on RELAY_CIRCUIT_UNRECOGNIZED_DATA_RECEIVED");
            return PLUGIN_RUN_DEFAULT;
          }
          ctx->plugin = ep->plugin;
          log_debug(LD_PLUGIN, "Running plugin entry point %s", key->entry_name);
          return plugin_run(ep, ctx, sizeof(relay_process_edge_t*));
        }
      case RELAY_PROCESS_EDGE_UNKNOWN:
      case RELAY_REPLACE_PROCESS_EDGE_SENDME:
      case RELAY_REPLACE_STREAM_DATA_RECEIVED:
      case RELAY_SENDME_CIRCUIT_DATA_RECEIVED:
        {
          /** probably need to pass a ctx of many interesting things */
          struct relay_process_edge_t *ctx = (relay_process_edge_t *) args;
          ctx->plugin = found->plugin;
          ctx->param = found->param;
          return plugin_run(found->entry_point, ctx, sizeof(relay_process_edge_t*));
        }
      case CIRCPAD_PROTOCOL_INIT:
      case CIRCPAD_PROTOCOL_MACHINEINFO_SETUP:
      case CIRCPAD_EVENT_CIRC_HAS_BUILT:
      case CIRCPAD_EVENT_CIRC_HAS_OPENED:
        {
          circpad_plugin_args_t *ctx = (circpad_plugin_args_t *) args;
          ctx->plugin = found->plugin;
          return plugin_run(found->entry_point, ctx, sizeof(circpad_plugin_args_t*));
        }
      case CIRCPAD_SEND_PADDING_CALLBACK:
        {
          // Look for plugin within this circuit connection context TODO
          circpad_plugin_args_t *ctx = (circpad_plugin_args_t *) args;
          /** This is a hook static value for connection specific plugins 
           *  Ideally this value should be CIRCPAD_SEND_PADDING_CALLBACK*/
          uint64_t uid = (uint64_t) CIRCPAD_SEND_PADDING_CALLBACK;
          tor_assert(ctx->circ);
          plugin_t *foundp = circuit_plugin_get(ctx->circ, uid);
          if (!foundp) {
            log_debug(LD_PLUGIN, "We didn't find a plugin with uid %lu on circ (%p) in circuit state %s with global id %ld",
                uid, ctx->circ, circuit_state_to_string(ctx->circ->state), ctx->circ->n_chan->global_identifier);
            return PLUGIN_RUN_DEFAULT;
          }
          ctx->plugin = foundp;
          plugin_entry_point_t *ep = plugin_get_entry_point_by_entry_name(foundp, key->entry_name);
          if (!ep) {
            log_debug(LD_PLUGIN, "We didn't find entry point name %s over plugin uid %lu for circ (%p)",
                key->entry_name, uid, ctx->circ);
            return PLUGIN_RUN_DEFAULT;
          }
          log_debug(LD_PLUGIN, "Running plugin entry point %s", key->entry_name);
          return plugin_run(ep, ctx, sizeof(circpad_plugin_args_t*));
        }
      case CONNECTION_EDGE_ADD_TO_SENDING_BEGIN:
      case RELAY_RECEIVED_CONNECTED_CELL:
        {
          conn_edge_plugin_args_t *ctx = (conn_edge_plugin_args_t *) args;
          ctx->plugin = found->plugin;
          ctx->param = found->param;
          return plugin_run(found->entry_point, ctx, sizeof(conn_edge_plugin_args_t *));
        }
      case PLUGIN_HOUSEKEEPING_INIT:
      case PLUGIN_HOUSEKEEPING_CLEANUP_CALLED:
        {
          plugin_plugin_args_t *ctx = (plugin_plugin_args_t*) args;
          if (ctx->plugin) {
            plugin_entry_point_t *ep = plugin_get_entry_point_by_entry_name(ctx->plugin, key->entry_name);
            if (!ep) {
              log_debug(LD_PLUGIN, "We didn't find entry point name %s over plugin uid %lu",
                  key->entry_name, ctx->plugin->uid);
              return PLUGIN_RUN_DEFAULT;
            }
            log_debug(LD_PLUGIN, "Running plugin entry point %s", key->entry_name);
            return plugin_run(ep, ctx, sizeof(plugin_plugin_args_t*));
          }
          else {
            log_debug(LD_PLUGIN, "Plugin missing from ctx");
            return PLUGIN_RUN_DEFAULT;
          }
        }
      default:
        log_debug(LD_PLUGIN, "Caller not found! %d:%s", caller,
            plugin_caller_id_to_string(caller));
        return -1; break;
      }
    }
  else {
    /** default code */
    log_debug(LD_PLUGIN, "Plugin not found: ptype:%d, putype:%d, pfamily:%d, entry_name:%s, param: %d", key->ptype, key->putype,
        key->pfamily, key->entry_name, key->param);
    return PLUGIN_RUN_DEFAULT;
  }
}

/**
 * Build a simplistic plug_cell_t that would just ask the peer to plug
 * a plugin with a given uid. We assume the peer to already hold said plugin.
 *
 * Ideally we should add a cell to reply and confirm or deny the demand. But we
 * don't need this engineering for this simple prototype. E.g., this could be
 * useful to actually send the plugin to the peer if they don't already have it
 * in a cache.
 *
 * This would require more protocol logic (easily set up with trunnel), and
 * also some bufferization logic to receive the plugin within multiple cells. This
 * is more complex, and could involve more research, like adding padding to
 * hide what plugin is sent to the peer from any network observer who is
 * counting cells.
 */

STATIC inline ssize_t build_plug_cell_v0(uint64_t uid, uint8_t *payload) {
  plug_cell_t *cell = NULL;
  ssize_t len = -1;

  cell = plug_cell_new();
 
  plug_cell_set_version(cell, 0);

  plug_cell_set_uid(cell, uid);

  plug_cell_set_length(cell, 0);
  /** Ok we can encode the structure within the RELAY cell payload */
  len = plug_cell_encode(payload, RELAY_PAYLOAD_SIZE, cell);

  plug_cell_free(cell);

  return len;
}

/**
 * Sending a RELAY_COMMAND_PLUG -- Currently only supports this from
 * the client.
 */

int send_plug_cell_v0_to_hop(origin_circuit_t *circ, uint64_t uid,
    uint8_t hopnum) {
  crypt_path_t *target_hop = circuit_get_cpath_hop(circ, hopnum);
  uint8_t payload[RELAY_PAYLOAD_SIZE];
  ssize_t payload_len;

  log_debug(LD_PLUGIN, "Sending plug cell containing uid %ld to hop num %d", uid, hopnum);

  /* Let's ensure we have it */ 
  if (!target_hop) {
    log_debug(LD_PLUGIN, "Circuit %u has %d hops, not %d. Why are we sending a plug cell?",
        circ->global_identifier, circuit_get_cpath_len(circ), hopnum);
    return -1;
  }
  
  /* only send the plug cell if open? */
  if (target_hop->state != CPATH_STATE_OPEN) {
    log_debug(LD_PLUGIN, "Circuit %u has %d hops, not %d. Why are we sending a plug cell?",
        circ->global_identifier, circuit_get_cpath_opened_len(circ), hopnum);
    return -1;
  }
  
  payload_len = build_plug_cell_v0(uid, payload);

  /*Let's send it */
  return relay_send_command_from_edge(0, TO_CIRCUIT(circ), RELAY_COMMAND_PLUG,
      (char *) payload, payload_len, target_hop);

}

int plugin_process_plug_cell(circuit_t *circ, const uint8_t *cell_payload,
    uint16_t cell_payload_len) {

  plug_cell_t *cell = NULL;

  if (CIRCUIT_IS_ORIGIN(circ)) {
    /* Not supported for this proto */
    log_debug(LD_PLUGIN, "NOT SUPPORTED YET");
    return -END_CIRC_REASON_TORPROTOCOL;
  }
  
  if (plug_cell_parse(&cell, cell_payload, cell_payload_len) < 0) {
    log_debug(LD_PLUGIN, "The plug_cell_t seems invalid");
    return -END_CIRC_REASON_TORPROTOCOL;
  }
  uint8_t version = plug_cell_get_version(cell);
  /** Currently we only support plugin a uid that we already have */
  tor_assert(!version);
  uint64_t uid = plug_cell_get_uid(cell);
  
  /**check that this plugin already
   * exists for this circ */
  SMARTLIST_FOREACH_BEGIN(circ->plugins, plugin_t*, plugin) {
    if (plugin->uid == uid) {
      log_debug(LD_PLUGIN, "Looks like we already have a plugin with %lu", plugin->uid);
      goto cleanup;
    }
  } SMARTLIST_FOREACH_END(plugin);

  /** let's find this uid and plug it on this circ
   * This is a connection-specific plugin that has j
   * just been instantiatied*/
  plugin_t *plugin = plugin_helper_find_from_uid_and_init(uid);
  tor_assert(plugin);
  /** add the plugin to the circ */
  smartlist_add(circ->plugins, plugin);
  /** invoke init if any */
  caller_id_t caller = PLUGIN_HOUSEKEEPING_INIT;
  plugin_plugin_args_t args;
  args.plugin = plugin;
  args.circ = circ;
  entry_point_map_t pmap;
  memset(&pmap, 0, sizeof(pmap));
  pmap.entry_name = (char*) "plugin_init";
  log_debug(LD_PLUGIN, "loaded and added plugin %ld to the circ (%p)."
                       " Calling its init function, to circ with global id %ld",
                       uid, circ, circ->n_chan->global_identifier);
  invoke_plugin_operation_or_default(&pmap, caller, (void*) &args);
cleanup:
  plug_cell_free(cell);
  return 0;
}


static int plugin_is_caller_id_system_wide(caller_id_t caller) {
  // ugly right.but caller_id_t should be < 2**16, and I am fed up with warnings
  switch((uint16_t)caller) {
    case PLUGIN_HOUSEKEEPING_INIT:
    case PLUGIN_HOUSEKEEPING_CLEANUP_CALLED:
    case CIRCPAD_SEND_PADDING_CALLBACK:
    case RELAY_CIRCUIT_UNRECOGNIZED_DATA_RECEIVED:
      return 0;
    default:
      return 1;
  }
}

static uint64_t util_get(int key, va_list *arguments) {
  
  switch (key) {
    case UTIL_CELL_PAYLOAD:
      {
        cell_t *cell = va_arg(*arguments, cell_t *);
        return (uint64_t) cell->payload;
      }
    case UTIL_CIRCUIT_IS_ORIGIN:
      {
        circuit_t *circ = va_arg(*arguments, circuit_t*);
        if (CIRCUIT_IS_ORIGIN(circ))
            return 1;
        break;
      }
    case UTIL_IS_RELAY:
      {
        return server_mode(get_options());
      }
    case UTIL_CONN_CTX:
      {
        circuit_t *circ = va_arg(*arguments, circuit_t*);
        return (uint64_t) circ->p_conn_ctx;
      }
    default:
      return 0;
  }
  return 0;
}

static void util_set(int key, va_list *arguments) {
  switch (key) {
    case UTIL_CONN_CTX:
      {
        circuit_t *circ = va_arg(*arguments, circuit_t *);
        void *ctx = va_arg(*arguments, void *);
        circ->p_conn_ctx = ctx;
      }
  }
}

static uint64_t plugin_get_arg(int key, va_list *arguments) {
  switch (key) {
    case PLUGIN_ARG_PLUGIN_T:
      {
        plugin_plugin_args_t *args = va_arg(*arguments, plugin_plugin_args_t *);
        return (uint64_t) args->plugin;
      }
    case PLUGIN_ARG_CIRCUIT_T:
      {
        plugin_plugin_args_t *args = va_arg(*arguments, plugin_plugin_args_t *);
        return (uint64_t) args->circ;
      }
     default: return 0;
  }
}

/**
 * get and set API access to the plugins
 *
 */

uint64_t get(int key, int arglen, ...) {
  va_list arguments;
  va_start(arguments, arglen);
  uint64_t ret = 0;
  if (key < RELAY_MAX) {
    ret = relay_get(key, &arguments);
  }
  else if (key < CIRCPAD_MAX) {
    ret = circpad_get(key, &arguments);
  }
  else if (key < CONNEDGE_MAX) {
    ret = connedge_get(key, &arguments);
  }
  else if (key < OPTIONS_MAX) {
    ret = options_get(key, &arguments);
  }
  else if (key < UTIL_MAX) {
    ret = util_get(key, &arguments);
  }
  else if (key < CIRCUIT_MAX) {
    ret = circuit_get(key, &arguments);
  }
  else if (key < PLUGIN_MAX) {
    ret = plugin_get_arg(key, &arguments);
  }
  va_end(arguments);
  return ret;
}

static void plugin_set(int key, va_list *arguments) {
  switch (key) {
    case PLUGIN_CTX:
      {
        plugin_plugin_args_t *args = va_arg(*arguments, plugin_plugin_args_t*);
        uint64_t val = va_arg(*arguments, uint64_t);
        args->plugin->ctx = (void *) val;
        break;
      }
  }
}

void set(int key, int arglen, ...) {
  va_list arguments;
  va_start(arguments, arglen);
  if (key < RELAY_MAX) {
    relay_set(key, &arguments);
  }
  else if (key < CIRCPAD_MAX) {
    circpad_set(key, &arguments);
  }
  else if (key < CONNEDGE_MAX) {
    connedge_set(key, &arguments);
  }
  else if (key < OPTIONS_MAX) {
  }
  else if (key < UTIL_MAX) {
    util_set(key, &arguments);
  }
  else if (key < CIRCUIT_MAX) {
    circuit_set(key, &arguments);
  }
  else if (key < SIGNAL_MAX) {
    signal_set(key, &arguments);
  }
  else if (key < PLUGIN_MAX) {
    plugin_set(key, &arguments);
  }
  va_end(arguments);
}

/**
 * Indirect call to functions
 *
 * Call a host function from some plugin
 * XXX Ideally we should do input sanitization
 */
int call_host_func(int keyfunc, int size, ...) {
  va_list arguments;
  int ret = 0;
  va_start(arguments, size);
  switch (keyfunc) {
    case RELAY_APPEND_CELL_TO_CIRCUIT_QUEUE:
      {
        circuit_t *circ = va_arg(arguments, circuit_t *);
        channel_t *chan = va_arg(arguments, channel_t *);
        cell_t *cell = va_arg(arguments, cell_t*);
        cell_direction_t direction;
        if (!CIRCUIT_IS_ORIGIN(circ)) {
          log_debug(LD_PLUGIN, "Marking direction in");
          direction = CELL_DIRECTION_IN;
        }
        else
          direction = CELL_DIRECTION_OUT;
        circpad_deliver_unrecognized_cell_events(circ, direction);
        log_debug(LD_PLUGIN, "Plugin: Appending cell to circuit queue");
        append_cell_to_circuit_queue(circ, chan, cell, direction, 0);
        break;
      }
    case RELAY_SEND_COMMAND_FROM_EDGE:
      {
        circuit_t *circ = va_arg(arguments, circuit_t*);
        uint32_t command = va_arg(arguments, uint32_t);
        crypt_path_t *layer_hint = va_arg(arguments, crypt_path_t *);
        ret = relay_send_command_from_edge(0, circ, command, 
            NULL, 0, layer_hint);
        break;

      }
    case CIRCPAD_REGISTER_PADDING_MACHINE:
      {
        circpad_machine_spec_t *machine = va_arg(arguments, circpad_machine_spec_t *);
        machine->is_plugin_generated = 1; /* the plugin may already did it, but we do it again for safety */
        smartlist_t *machine_sl = va_arg(arguments, smartlist_t *);
        circpad_register_padding_machine(machine, machine_sl);
        ret = 0;
        break;
      }
    case CIRCPAD_MACHINE_STATES_INIT:
      {
        circpad_machine_spec_t *machine = va_arg(arguments, circpad_machine_spec_t *);
        int nbr_states = va_arg(arguments, int);
        circpad_machine_states_init(machine, nbr_states);
        ret = 0;
        break;
      }
    case CIRCPAD_CIRC_PURPOSE_TO_MASK:
      {
        uint32_t purpose = va_arg(arguments, uint32_t);
        ret = circpad_circ_purpose_to_mask((uint8_t) purpose);
        break;
      }
    case CIRCPAD_MACHINE_SPEC_TRANSITION:
      {
        circpad_machine_runtime_t *mr = va_arg(arguments, circpad_machine_runtime_t *);
        circpad_event_t event = va_arg(arguments, circpad_event_t);
        circpad_machine_spec_transition(mr, event);
        ret = 0;
        break;
      }
    case CIRCPAD_SEND_COMMAND_TO_MIDDLE_HOP:
      {
        origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(va_arg(arguments, circuit_t*));
        uint8_t hop = 2;
        uint8_t command = 255; // should not be used by any core protocol feature
        uint8_t *payload = va_arg(arguments, uint8_t*);
        ssize_t len = va_arg(arguments, ssize_t);
        if (circpad_send_command_to_hop(ocirc, hop, command, payload, len)) {
          log_debug(LD_PLUGIN, "Failed to send command %d at hop %d, with length %lu", command, hop, len);
        }
        ret = 0;
        break;
      }
    case CIRCPAD_SEND_COMMAND_TO_GUARD:
      {
        origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(va_arg(arguments, circuit_t*));
        uint8_t hop = 1;
        uint8_t command = 255; // should not be used by any core protocol feature
        uint8_t *payload = va_arg(arguments, uint8_t*);
        ssize_t len = va_arg(arguments, ssize_t);
        if (circpad_send_command_to_hop(ocirc, hop, command, payload, len)) {
          log_debug(LD_PLUGIN, "Failed to send command %d at hop %d, with length %lu", command, hop, len);
        }
        ret = 0;
        break;
      }
    case CIRCPAD_CHECK_MACHINE_TOKEN_SUPPLY:
    case CIRCPAD_MACHINE_COUNT_PADDING_SENT:
      {
        return call_static_circpad_func(keyfunc, &arguments);
      }
    case CIRCPAD_SEND_COMMAND_DROP_TO_HOP:
      {
        origin_circuit_t *ocirc = TO_ORIGIN_CIRCUIT(va_arg(arguments, circuit_t*));
        uint8_t hop = va_arg(arguments, uint32_t);
        if (circpad_send_command_to_hop(ocirc, hop, RELAY_COMMAND_DROP, NULL, 0)) {
          log_debug(LD_PLUGIN, "Failed to send command DROP at hop %d", hop);
          ret = -1;
        }
        ret = 0;
        break;
      }
    case CIRCPAD_CELL_EVENT_PADDING_SENT:
      {
        circuit_t *circ = va_arg(arguments, circuit_t*);
        circpad_cell_event_padding_sent(circ);
        break;
      }
    case TIMER_DISABLE:
      {
        tor_timer_t *timer = va_arg(arguments, tor_timer_t*);
        timer_disable(timer);
        break;
      }
    case PLUGIN_SEND_PLUG_CELL:
      {
        circuit_t *circ = va_arg(arguments, circuit_t *);
        uint64_t uid = va_arg(arguments, uint64_t);
        uint32_t hopnum = va_arg(arguments, uint32_t);
        tor_assert(CIRCUIT_IS_ORIGIN(circ));
        send_plug_cell_v0_to_hop(TO_ORIGIN_CIRCUIT(circ), uid, (uint8_t) hopnum);
        break;
      }
    case PLUGIN_CLEANUP_CIRC:
      {
        circuit_t *circ  = va_arg(arguments, circuit_t *);
        uint64_t uid = va_arg(arguments, uint64_t);
        plugin_cleanup_conn(circ, uid);
        break;
      }
  }
  va_end(arguments);
  return ret;
}

entry_point_map_t *plugin_get(entry_point_map_t *key) {
  entry_point_map_t *found;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, key);
  return found;
}

plugin_entry_point_t *plugin_get_entry_point_by_entry_name(plugin_t *plugin, char
    *entry_name) {
  SMARTLIST_FOREACH_BEGIN(plugin->entry_points, plugin_entry_point_t *, ep) {
    if (!strcmp(ep->entry_name, entry_name))
      return ep;
  } SMARTLIST_FOREACH_END(ep);
  return NULL;
}

void plugin_map_entrypoint_remove(plugin_entry_point_t *ep) {
  entry_point_map_t *found = NULL;
  entry_point_map_t search;
  search.entry_name = tor_strdup(ep->entry_name);
  search.ptype = ep->info.ptype;
  search.putype = ep->info.putype;
  search.pfamily = ep->info.pfamily;
  search.param = ep->info.param;
  found = HT_REMOVE(plugin_map_ht, &plugin_map_ht, &search);
  if (!found) {
    log_debug(LD_PLUGIN, "NOT FOUND: Trying to remove a plugin from the hashmap %s", search.entry_name);
  }
  else {
    tor_free(found->entry_name);
    tor_free(found);
  }
  tor_free(search.entry_name);
}

void plugin_cleanup_conn(circuit_t *circ, uint64_t uid) {
  caller_id_t caller = PLUGIN_HOUSEKEEPING_CLEANUP_CALLED;
  plugin_plugin_args_t args;
  memset(&args, 0, sizeof(args));
  args.circ = circ;
  log_debug(LD_PLUGIN, "Cleaning up connection plugin. Invoking plugin's housekeeping code");
  plugin_t *plugin = circuit_plugin_get(circ, uid);
  if (plugin) {
    args.plugin = plugin;
    entry_point_map_t pmap;
    memset(&pmap, 0, sizeof(pmap));
    pmap.entry_name = (char *) "plugin_cleanup";
    invoke_plugin_operation_or_default(&pmap, caller, (void*) &args);
    plugin_unplug(plugin);
    smartlist_remove(circ->plugins, plugin);
  }
}

uint64_t plugin_run(plugin_entry_point_t *entry_point, void *args, size_t args_size) {
  uint64_t ret = exec_loaded_code(entry_point, args, args_size);
  if (ret) {
    log_debug(LD_PLUGIN, "Plugin execution returned %ld", ret);
    const char *errormsg = ubpf_get_error_msg(entry_point->vm);
    log_debug(LD_PLUGIN, "vm error message: %s", errormsg);
  }
  return ret;
}

