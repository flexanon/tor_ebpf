/**
 * \file plugin.h
 * \brief Header file for plugin.c
 **/

#ifndef  PLUGIN_H
#define PLUGIN_H

#include "core/or/or.h"
#include "ubpf/vm/inc/ubpf.h"
#include <stdarg.h>

#define PLUGIN_RUN_DEFAULT -2147483648
/**
 * Define the type of usage the plugin is intended to.
 * We may want to replace a functionality to perform a same action
 * We may want to simply remove a functionality
 * We may want to add a new functionality to existing code
 */
typedef enum {
  PLUGIN_CODE_HIJACK,
  PLUGIN_CODE_ADD,
  PLUGIN_CODE_DEL
} plugin_usage_type_t;

typedef enum {
  PLUGIN_DEV,
  PLUGIN_USER
} plugin_type_t;

/**
 * Various type of Protocols that can be hijacked, where functions can be added
 * or removed.
 *
 * Note, this is still very early prototyping, we do not support much things yet
 *
 * PLUGIN_PROTOCOL_CORE is expected to refer to the core framework that can only
 * be hijacked be the main developers. Hijacking PROTOCOL_CORE would allow the
 * devs to re-write basically anything.
 */

typedef enum {
  PLUGIN_PROTOCOL_CORE,
  PLUGIN_PROTOCOL_RELAY,
  PLUGIN_PROTOCOL_CIRCPAD,
  PLUGIN_PROTOCOL_CONN_EDGE
} plugin_family_t;

typedef struct entry_info_t {
  char *entry_name;
  plugin_type_t ptype;
  plugin_usage_type_t putype;
  plugin_family_t pfamily;
  plugin_t *plugin;
  int param;
} entry_info_t;

/** parameters given to the plugin when executed */
typedef struct plugin_args_t {
  int param; /** empty for now */
} plugin_args_t;

/**
 * Who's calling us? Will be used to prepare plugin_run()
 */
typedef enum {
  /** Replace circuit_sendme logic */
  RELAY_REPLACE_PROCESS_EDGE_SENDME,
  RELAY_REPLACE_STREAM_DATA_RECEIVED,
  /** We received a cell that is not part of the current relay protocol version*/
  RELAY_PROCESS_EDGE_UNKNOWN,
  /** We have one or several circpad machines to globally add to all circuits */
  CIRCPAD_PROTOCOL_INIT,
  /**Conn edge stuffs */
  CONNECTION_EDGE_ADD_TO_SENDING_BEGIN
} caller_id_t;

typedef struct entry_point_map_t {
  HT_ENTRY(entry_point_map_t) node;
  /** should the same as entry_point->entry_name */
  char *entry_name;
  plugin_type_t ptype;
  plugin_usage_type_t putype;
  plugin_family_t pfamily;
  int param;
  plugin_entry_point_t *entry_point;
  plugin_t *plugin;
} entry_point_map_t;

#define PLUGIN_CTX 1
  /**
   * Access main objects of process_edge
   */
/** get circuit_t* */
#define RELAY_CIRCUIT_T 10
/*  get crypt_path_t*  */
#define RELAY_CRYPT_PATH_T 11
/*  get cell_t* */
#define RELAY_CELL_T 12


// TODO We need something that can be loaded from some state file instead!
/** Accessible field elements */

#define RELAY_LAYER_HINT_DELIVER_WINDOW 13
#define RELAY_CIRC_DELIVER_WINDOW 14
#define RELAY_CONN_DELIVER_WINDOW 15
#define RELAY_PLUGIN_CTX 16

#define RELAY_MAX 1000

/** Circpad related field elements */
#define CIRCPAD_MACHINE_LIST_SIZE 1001
#define CIRCPAD_CLIENT_MACHINES_SL 1002
#define CIRCPAD_RELAY_MACHINES_SL 1003
#define CIRCPAD_PLUGIN_T 1004
#define CIRCPAD_NEW_EVENTNUM 1005
#define CIRCPAD_PLUGIN_CTX 1006

#define CIRCPAD_MAX 2000

/*** KEYFUNC */

#define RELAY_SEND_COMMAND_FROM_EDGE 1
#define CIRCPAD_MACHINE_REGISTER 2

#define RELAY_KEYFUNC_MAX 100

#define CIRCPAD_REGISTER_PADDING_MACHINE 101
#define CIRCPAD_MACHINE_STATES_INIT 102
#define CIRCPAD_CIRC_PURPOSE_TO_MASK 103

#define CIRCPAD_KEYFUNC_MAX 200

/**
 * Authentify and load plugins from $(data_directory)/plugins
 * @elf_name plugin name 
 */
void plugin_init(char *elf_name);

/**
 */
int plugin_plug_elf(plugin_t *plugin, entry_info_t *pinfo, char* elfpath);

int invoke_plugin_operation_or_default(entry_point_map_t *pmap, caller_id_t caller, void *args);

uint64_t plugin_run(plugin_entry_point_t *plugin, void *args, size_t size);

entry_point_map_t *plugin_get(entry_point_map_t *key);

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
int call_host_func(int key, int size, ...);

#endif
