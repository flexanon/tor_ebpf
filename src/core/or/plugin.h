/**
 * \file plugin.h
 * \brief Header file for plugin.c
 **/

#ifndef  PLUGIN_H
#define PLUGIN_H

#include "core/or/or.h"
#include "ubpf/vm/inc/ubpf.h"

/**
 * Define the type of usage the plugin is intended to.
 * We may want to replace a functionality to perform a same action
 * We may want to simply remove a functionality
 * We may want to add a new functionality to existing code
 */
typedef enum {
  PLUGIN_CODE_HIGHJACK,
  PLUGIN_CODE_ADD,
  PLUGIN_CODE_DEL
} plugin_usage_type_t;

typedef enum {
  PLUGIN_DEV,
  PLUGIN_USER
} plugin_type_t;

typedef enum {
  PLUGIN_PROTOCOL_RELAY
} plugin_family_t;

/* Contains the information for all code accessible
 * inside a plugin directory.
 * e.g., plugins/relay_protocol may have many
 * .o files with each of them one entry point 
 */
typedef struct plugin_info_list_t {
  char *pname;
  /** may have many plugins */
  smartlist_t *subplugins;
}plugin_info_list_t;

typedef struct plugin_info_t {
  char *subname;
  plugin_type_t ptype;
  plugin_usage_type_t putype;
  plugin_family_t pfamily;
  plugin_t *plugin;
  int param;
  size_t memory_needed;
} plugin_info_t;

/** parameters given to the plugin when executed */
typedef struct plugin_args_t {
  int param; /** empty for now */
} plugin_args_t;

typedef enum {
  /** Replace circuit_sendme logic */
  RELAY_REPLACE_PROCESS_EDGE_SENDME,
  /** We received a cell that is not part of the current relay protocol version*/
  RELAY_PROCESS_EDGE_UNKNOWN
} caller_id_t;

typedef struct plugin_map_t {
  HT_ENTRY(plugin_map_t) node;
  char *subname;
  plugin_type_t ptype;
  plugin_usage_type_t putype;
  plugin_family_t pfamily;
  size_t memory_size;
  plugin_t *plugin;
  int param;
} plugin_map_t;

  /**
   * Access main objects of process_edge
   */
/** get circuit_t* */
#define RELAY_CIRCUIT_T 1
/*  get crypt_path_t*  */
#define RELAY_LAYER_HINT_T 2
/*  get cell_t* */
#define RELAY_CELL_T 3


/** Accessible field elements */

#define RELAY_LAYER_HINT_DELIVER_WINDOW 4
#define RELAY_CIRC_DELIVER_WINDOW 5

#define RELAY_MAX 1000

/*** KEYFUNC */

#define RELAY_SEND_COMMAND_FROM_EDGE 1

#define RELAY_KEYFUNC_MAX 100

/**
 * Authentify and load plugins from $(data_directory)/plugins
 * @elf_name plugin name 
 */
void plugin_init(char *elf_name);

/**
 */
int plugin_plug_elf(plugin_info_t *pinfo, char* elfpath);

/**
 */
void plugin_unplug(plugin_info_list_t *list_plugins);

int invoke_plugin_operation_or_default(plugin_map_t *pmap, caller_id_t caller, void *args);

uint64_t plugin_run(plugin_t *plugin, void *args, size_t size);

/**********************************PLUGIN API***************************/

/**
 * API accessible from plugins
 *
 */

uint64_t get(int key, void *pointer);

void set(int key, void *pointer, uint64_t val);

/** used to call function with more than 5 parameters
 * 
 * -- UBPF limitation-- */
int call_host_func(int key, void *args);

#endif
