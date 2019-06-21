/*y
 * \file plugin.c
 * \brief Handle plugin main operations, implement the API definition to
 * interact with plugins
 **/

#include "core/or/or.h"
#include "app/config/config.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "core/or/relay.h"
#include "ubpf/vm/inc/ubpf.h"


/**
 * Hashtable containing plugin information 
 */


static inline int
plugin_entries_eq_(plugin_map_t *a, plugin_map_t *b) {
  return !strcmp(a->subname, b->subname) && a->param == b->param &&
         a->ptype == b->ptype && a->putype == b->putype &&
         a->pfamily == b->pfamily && a->memory_size == b->memory_size;
}

static inline unsigned int
plugin_entry_hash_(plugin_map_t *a) {

  uint32_t array[5+256/4] = {0,}; /** putype+ptype+ max name authorized*/
  array[0] = a->ptype;
  array[1] = a->putype;
  array[2] = a->pfamily;
  array[3] = a->memory_size; //64bits
  memcpy(&array[5], a->subname, strlen(a->subname));
  return (unsigned) siphash24g(array, sizeof(array));
}

static HT_HEAD(plugin_map_ht, plugin_map_t)
      plugin_map_ht = HT_INITIALIZER();
HT_PROTOTYPE(plugin_map_ht, plugin_map_t, node,
             plugin_entry_hash_, plugin_entries_eq_)
HT_GENERATE2(plugin_map_ht, plugin_map_t, node,
             plugin_entry_hash_, plugin_entries_eq_, 0.6,
             tor_reallocarray_, tor_free_)

/****************************************************************/

int plugin_plug_elf(plugin_info_t *pinfo, char *elfpath) {
  /** Todo verif* if this plugin is not already in our map */
  plugin_map_t search;
  plugin_map_t *found;
  search.subname = tor_strdup(pinfo->subname);
  search.ptype = pinfo->ptype;
  search.putype = pinfo->putype;
  search.pfamily = pinfo->pfamily;
  search.param = pinfo->param;
  search.memory_size = pinfo->memory_needed;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, &search);
  if (found) {
    /** TODO */
  }
  else {
    plugin_t *plugin = load_elf_file(elfpath,  pinfo->memory_needed);
    if (!plugin) {
      log_debug(LD_PLUGIN, "Failed to load plugin at elfpath %s, with heap of size %lu bytes", elfpath,
          pinfo->memory_needed);
      return -1;
    }
    found = tor_malloc_zero(sizeof(plugin_map_t));
    pinfo->plugin = plugin; /** careful double free */
    found->plugin = plugin;
    found->subname = tor_strdup(pinfo->subname);
    found->putype = pinfo->putype;
    found->pfamily = pinfo->pfamily;
    found->ptype = pinfo->ptype;
    found->memory_size = pinfo->memory_needed;
    /** Register the plugin; do it for each family*/
    found->param = pinfo->param;
    HT_INSERT(plugin_map_ht, &plugin_map_ht, found);
    log_debug(LD_PLUGIN, "Inserted plugin in map");
  }
  return 0;
}

/**
 * Given parameters, look into loaded plugins if one
 * matches. If not, call default code. 
 */

int invoke_plugin_operation_or_default(plugin_map_t *key,
    caller_id_t caller, void *args) {
  if (!get_options()->EnablePlugins) {
    log_debug(LD_PLUGIN, "Plugins not enabled; defaulting to existing code");
    return -1;
  }
  plugin_map_t *found;
  found = HT_FIND(plugin_map_ht, &plugin_map_ht, key);
  if (found) {
    log_debug(LD_PLUGIN, "Invoking plugin operation");
    switch (caller) {
      case RELAY_REPLACE_PROCESS_EDGE_SENDME:
        {
          log_debug(LD_PLUGIN, "Plugin found for caller %s",
              plugin_caller_id_to_string(caller));
          struct relay_process_edge_t *ctx = (relay_process_edge_t*) args;
          return plugin_run(found->plugin, ctx, sizeof(relay_process_edge_t));
        }
    }
  }
  else {
    /** default code */
    log_debug(LD_PLUGIN, "Plugin not found: ptype:%d, putype:%d, pfamily:%d, memory_size:%d, subname:%s, param: %d", key->ptype, key->putype,
        key->pfamily, key->memory_size, key->subname, key->param);
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
  return NULL;
}

void set(int key, void *pointer, uint64_t val) {
  if (key < RELAY_MAX) {
    relay_set(key, pointer, val);
  }
}

int call_host_func(int keyfunc, void *args) {
  switch (keyfunc) {
    case RELAY_SEND_COMMAND_FROM_EDGE:
      {
        relay_process_edge_t *pedge = (relay_process_edge_t*) args;
        return relay_send_command_from_edge(0, pedge->circ, RELAY_COMMAND_SENDME, 
            NULL, 0, pedge->layer_hint);

      }
  }
  return -1;
}

uint64_t plugin_run(plugin_t *plugin, void *args, size_t args_size) {
  uint64_t ret = exec_loaded_code(plugin, args, args_size);
  if (ret) {
    log_debug(LD_PLUGIN, "Plugin execution returned %ld", ret);
    char *errormsg = ubpf_get_error_msg(plugin->vm);
    log_debug(LD_PLUGIN, "vm error message: %s", errormsg);
  }
  return ret;
}

/**
 * TODO
 */

void plugin_unplug(plugin_info_list_t *list_plugins) {
  (void)list_plugins;
}

