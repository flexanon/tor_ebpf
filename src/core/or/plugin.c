/*y
 * \file plugin.c
 * \brief Handle plugin main operations, implement the API definition to
 * interact with plugins
 **/

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "core/or/relay.h"
#include "ubpf/vm/inc/ubpf.h"

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
    ret = load_elf_file(elfpath,  plugin, entry_point);
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
    log_debug(LD_PLUGIN, "Inserted plugin in map");
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
    return -1;
  }
  entry_point_map_t *found;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, key);
  if (found) {
    log_debug(LD_PLUGIN, "Invoking plugin operation");
    switch (caller) {
      case RELAY_REPLACE_PROCESS_EDGE_SENDME:
        {
          log_debug(LD_PLUGIN, "Plugin found for caller %s",
              plugin_caller_id_to_string(caller));
          struct relay_process_edge_t *ctx = (relay_process_edge_t*) args;
          ctx->plugin = found->plugin;
          return plugin_run(found->entry_point, ctx, sizeof(relay_process_edge_t));
        }
      case RELAY_PROCESS_EDGE_UNKNOWN:
        /** probably need to pass a ctx of many interesting things */
        return -1;
      case CIRCPAD_PROTOCOL_INIT:
        {
          log_debug(LD_PLUGIN, "Plugin found for caller %s", 
              plugin_caller_id_to_string(caller));
          circpad_plugin_args_t *ctx = (circpad_plugin_args_t *) args;
          ctx->plugin = found->plugin;
          return plugin_run(found->entry_point, ctx, sizeof(circpad_plugin_args_t));
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
    return -1;
  }
}

/**
 * get and set API access to the plugins
 *
 */

uint64_t get(int key, void *pointer) {
  if (key < RELAY_MAX) {
    return relay_get(key, pointer);
  }
  else if (key < CIRCPAD_MAX) {
    return circpad_get(key, pointer);
  }
  return 0;
}

void set(int key, void *pointer, uint64_t val) {
  if (key < RELAY_MAX) {
    relay_set(key, pointer, val);
  }
  else if (key < CIRCPAD_MAX) {
  }
}

int call_host_func(int keyfunc, int size, ...) {
  va_list arguments;
  int ret = 0;
  switch (keyfunc) {
    case RELAY_SEND_COMMAND_FROM_EDGE:
      {
        va_start(arguments, size);
        relay_process_edge_t *pedge = va_arg(arguments, relay_process_edge_t*);
        ret = relay_send_command_from_edge(0, pedge->circ, RELAY_COMMAND_SENDME, 
            NULL, 0, pedge->layer_hint);
        break;

      }
    case CIRCPAD_REGISTER_PADDING_MACHINE:
      {
        va_start(arguments, size);
        circpad_machine_spec_t *machine = va_arg(arguments, circpad_machine_spec_t *);
        machine->is_plugin_generated = 1; /* the plugin may already did it, but we do it again for safety */
        smartlist_t *machine_sl = va_arg(arguments, smartlist_t *);
        circpad_register_padding_machine(machine, machine_sl);
        ret = 0;
        break;
      }
    case CIRCPAD_MACHINE_STATES_INIT:
      {
        va_start(arguments, size);
        circpad_machine_spec_t *machine = va_arg(arguments, circpad_machine_spec_t *);
        int nbr_states = va_arg(arguments, int);
        circpad_machine_states_init(machine, nbr_states);
        ret = 0;
        break;
      }
    case CIRCPAD_CIRC_PURPOSE_TO_MASK:
      {
        va_start(arguments, size);
        uint32_t purpose = va_arg(arguments, uint32_t);
        ret = circpad_circ_purpose_to_mask((uint8_t) purpose);
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

uint64_t plugin_run(plugin_entry_point_t *entry_point, void *args, size_t args_size) {
  uint64_t ret = exec_loaded_code(entry_point, args, args_size);
  if (ret) {
    log_debug(LD_PLUGIN, "Plugin execution returned %ld", ret);
    const char *errormsg = ubpf_get_error_msg(entry_point->vm);
    log_debug(LD_PLUGIN, "vm error message: %s", errormsg);
  }
  return ret;
}

