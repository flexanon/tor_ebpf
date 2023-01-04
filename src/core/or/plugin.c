/*y
 * \file plugin.c
 * \brief Handle plugin main operations, implement the API definition to
 * interact with plugins
 **/

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/circuitpadding.h"
#include "core/or/connection_edge.h"
#include "core/or/circuit_st.h"
#include "core/or/circuitlist.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "core/or/relay.h"
#include "feature/relay/routermode.h"
#include "ubpf/vm/inc/ubpf.h"
#include <time.h>
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

/****************************************************************/

int plugin_plug_elf(plugin_t *plugin, entry_info_t *einfo, char *elfpath) {
  entry_point_map_t search;
  entry_point_map_t *found;
  search.entry_name = tor_strdup(einfo->entry_name);
  search.ptype = einfo->ptype;
  search.putype = einfo->putype;
  search.pfamily = einfo->pfamily;
  search.param = einfo->param;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, &search);
  if (found) {
    /** XXX What shoud we do?*/
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
  else {
    plugin_entry_point_t *entry_point = tor_malloc_zero(sizeof(*entry_point));
    int ret;
    clock_t start, end;
    double cpu_time_used;
    start = clock();
    ret = load_elf_file(elfpath,  plugin, entry_point);
    end = clock();
    cpu_time_used =  ((double) (end - start)) / CLOCKS_PER_SEC;
    log_info(LD_PLUGIN, "Loading Plugin entry_point %s took %f sec", search.entry_name, cpu_time_used);
    if (ret < 0) {
      log_debug(LD_PLUGIN, "Failed to load plugin at elfpath %s, with heap of size %lu bytes", elfpath,
          plugin->memory_size);
      tor_free(entry_point);
      return -1;
    }
    entry_point->entry_name = tor_strdup(einfo->entry_name);
    smartlist_add(plugin->entry_points, entry_point);
    found = tor_malloc_zero(sizeof(entry_point_map_t));
    found->plugin = plugin;
    found->entry_point = entry_point;
    /*take ownership */
    found->entry_name = einfo->entry_name;
    found->putype = einfo->putype;
    found->pfamily = einfo->pfamily;
    found->ptype = einfo->ptype;
    /** Register the plugin; do it for each family*/
    found->param = einfo->param;
    HT_INSERT(plugin_map_ht, &plugin_map_ht, found);
    log_debug(LD_PLUGIN, "Inserted plugin name:%s; putype:%d, ptype: %d,\
        pfamily:%d, param:%d in map", found->entry_name, found->putype,
        found->ptype, found->pfamily, found->param);
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
  entry_point_map_t *found;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, key);
  if (found) {
    log_debug(LD_PLUGIN, "Plugin found for caller %s",
              plugin_caller_id_to_string(caller));
    switch (caller) {
      case RELAY_CIRCUIT_UNRECOGNIZED_DATA_RECEIVED:
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
      case CIRCPAD_SEND_PADDING_CALLBACK:
      case CIRCPAD_EVENT_CIRC_HAS_OPENED:
        {
          circpad_plugin_args_t *ctx = (circpad_plugin_args_t *) args;
          ctx->plugin = found->plugin;
          return plugin_run(found->entry_point, ctx, sizeof(circpad_plugin_args_t*));
        }
      case CONNECTION_EDGE_ADD_TO_SENDING_BEGIN:
      case RELAY_RECEIVED_CONNECTED_CELL:
        {
          conn_edge_plugin_args_t *ctx = (conn_edge_plugin_args_t *) args;
          ctx->plugin = found->plugin;
          ctx->param = found->param;
          return plugin_run(found->entry_point, ctx, sizeof(conn_edge_plugin_args_t *));
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

static uint64_t util_get(int key, va_list *arguments) {
  
  switch (key) {
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
    default:
      return 0;
  }
  return 0;
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
  va_end(arguments);
  return ret;
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
  }
  else if (key < CIRCUIT_MAX) {
    circuit_set(key, &arguments);
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

uint64_t plugin_run(plugin_entry_point_t *entry_point, void *args, size_t args_size) {
  uint64_t ret = exec_loaded_code(entry_point, args, args_size);
  if (ret) {
    log_debug(LD_PLUGIN, "Plugin execution returned %ld", ret);
    const char *errormsg = ubpf_get_error_msg(entry_point->vm);
    log_debug(LD_PLUGIN, "vm error message: %s", errormsg);
  }
  return ret;
}

