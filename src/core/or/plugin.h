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
 * Who's calling us? Will be used to prepare plugin_run()
 *
 * XXX They should probably be all assigned and refer to
 * the plugin uid if there is a single function?
 */
typedef enum {
  /** Replace circuit_sendme logic */
  RELAY_REPLACE_PROCESS_EDGE_SENDME,
  RELAY_REPLACE_STREAM_DATA_RECEIVED,
  /** We received a cell that is not part of the current relay protocol version*/
  RELAY_PROCESS_EDGE_UNKNOWN,
  /** received a relay cell we can react to it */
  RELAY_RECEIVED_CONNECTED_CELL,
  /** we received some data -- let's tell the sendme alg */
  RELAY_SENDME_CIRCUIT_DATA_RECEIVED,
  /** we received some data that we don't recognized and would pass to the next
   * hop */
  RELAY_CIRCUIT_UNRECOGNIZED_DATA_RECEIVED,
  /** We have one or several circpad machines to globally add to all circuits */
  CIRCPAD_PROTOCOL_INIT,
  CIRCPAD_PROTOCOL_MACHINEINFO_SETUP,
  CIRCPAD_EVENT_CIRC_HAS_BUILT,
  CIRCPAD_EVENT_CIRC_HAS_OPENED,
  /** This is a connection-specific hook -- assgning an ID*/
  CIRCPAD_SEND_PADDING_CALLBACK = 42,
  /**Conn edge stuffs */
  CONNECTION_EDGE_ADD_TO_SENDING_BEGIN,

  PLUGIN_HOUSEKEEPING_CLEANUP_CALLED,
  PLUGIN_HOUSEKEEPING_INIT,
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

/**
 * Access main objects of process_edge
 */
/** get circuit_t* */
#define RELAY_ARG_CIRCUIT_T 10
/*  get crypt_path_t*  */
#define RELAY_ARG_CRYPT_PATH_T 11
/*  get cell_t* */
#define RELAY_ARG_CELL_T 12


// TODO We need something that can be loaded from some state file instead!
/** Accessible field elements */

#define RELAY_LAYER_HINT_DELIVER_WINDOW 13
#define RELAY_CIRC_DELIVER_WINDOW 14
#define RELAY_CONN_DELIVER_WINDOW 15
#define RELAY_PLUGIN_CTX 16
#define RELAY_ARG_PLUGIN_T 17
#define RELAY_ARG_PARAM 18
#define RELAY_ARG_CIRCUIT_CHAN_T 19
#define RELAY_ARG_CELL_DIRECTION_T 20

#define RELAY_MAX 1000

/** Circpad related field elements */
#define CIRCPAD_MACHINE_LIST_SIZE 1001
#define CIRCPAD_CLIENT_MACHINES_SL 1002
#define CIRCPAD_RELAY_MACHINES_SL 1003
/*circpad_plugin_args_t.plugin */
#define CIRCPAD_ARG_PLUGIN_T 1004
#define CIRCPAD_NEW_EVENTNUM 1005
#define CIRCPAD_PLUGIN_CTX 1006
#define CIRCPAD_MACHINE_RUNTIME 1007
#define CIRCPAD_PLUGIN_MACHINE_SPEC 1008
#define CIRCPAD_MACHINE_CTR 1009
/*circpad_plugin_args_t.machine */
#define CIRCPAD_ARG_MACHINE_SPEC_T 1010
/*circpad_plugin_args_t.machine_runtime */
#define CIRCPAD_ARG_MACHINE_RUNTIME 1011
/*circpad_machine_runtime_t.plugin_machine_runtime*/
#define CIRCPAD_PLUGIN_MACHINE_RUNTIME 1012
#define CIRCPAD_ARG_CIRCUIT_T 1013
#define CIRCPAD_MACHINE_RUNTIME_PADDING_SCHEDULED_AT_USEC 1014
#define CIRCPAD_MACHINE_RUNTIME_IS_PADDING_TIMER_SCHEDULED 1015
#define CIRCPAD_MACHINE_RUNTIME_STATE 1016
#define CIRCPAD_MAX_CIRC_QUEUED_CELLS 1017
#define CIRCPAD_MACHINE_SPEC_TARGET_HOP 1018
#define CIRCPAD_MACHINE_RUNTIME_STATELENGTH 1019
#define CIRCPAD_MACHINE_SPEC_NAME 1020
#define CIRCPAD_MACHINE_RUNTIME_PADDING_TIMER 1021

#define CIRCPAD_MAX 2000

#define CONNEDGE_ARG_CIRCUIT_T 2001
#define CONNEDGE_ARG_PLUGIN_T 2002
#define CONNEDGE_ARG_PARAM 2003
#define CONNEDGE_PLUGIN_CTX 2004

#define CONNEDGE_MAX 3000

#define OPTIONS_ORPORT_SET 3001

#define OPTIONS_MAX 4000

#define UTIL_CIRCUIT_IS_ORIGIN 4001
#define UTIL_IS_RELAY 4002
#define UTIL_CONN_CTX 4003
#define UTIL_CELL_PAYLOAD 4004

#define UTIL_MAX 5000

#define CIRCUIT_MARKED_FOR_CLOSE 5001
#define CIRCUIT_P_CHAN_QUEUED_CELLS 5002
#define CIRCUIT_N_CIRC_ID 5003

#define CIRCUIT_MAX 6000

#define PLUGIN_CTX 6001
#define PLUGIN_ARG_PLUGIN_T 6002
#define PLUGIN_ARG_CIRCUIT_T 6003

#define PLUGIN_MAX 7000
/*** KEYFUNC */

#define RELAY_SEND_COMMAND_FROM_EDGE 1
#define CIRCPAD_MACHINE_REGISTER 2
#define RELAY_APPEND_CELL_TO_CIRCUIT_QUEUE 3

#define RELAY_KEYFUNC_MAX 100

#define CIRCPAD_REGISTER_PADDING_MACHINE 101
#define CIRCPAD_MACHINE_STATES_INIT 102
#define CIRCPAD_CIRC_PURPOSE_TO_MASK 103
#define CIRCPAD_MACHINE_SPEC_TRANSITION 104
#define CIRCPAD_SEND_COMMAND_TO_MIDDLE_HOP 105
#define CIRCPAD_MACHINE_COUNT_PADDING_SENT 106
#define CIRCPAD_SEND_COMMAND_DROP_TO_HOP 107
#define CIRCPAD_CHECK_MACHINE_TOKEN_SUPPLY 108
#define CIRCPAD_CELL_EVENT_PADDING_SENT 109

#define CIRCPAD_KEYFUNC_MAX 200

#define TIMER_DISABLE 201

#define TIMER_KEYFUNC_MAX 300

#define PLUGIN_CLEANUP_CIRC 301
#define PLUGIN_SEND_PLUG_CELL 302
#define PLUGIN_KEYFUNC_MAX 400

/** What type of argument do we give to pluginized function
 *  in the plugin module? */
typedef struct plugin_plugin_args_t {
  circuit_t *circ;
  plugin_t *plugin;
} plugin_plugin_args_t;

/**
 * Authentify and load plugins from $(data_directory)/plugins
 * @elf_name plugin name 
 */
void plugin_init(char *elf_name);

/**
 */
int plugin_plug_elf(plugin_t *plugin, entry_info_t *pinfo, char* elfpath);

int invoke_plugin_operation_or_default(entry_point_map_t *pmap, caller_id_t caller, void *args);

int send_plug_cell_v0_to_hop(origin_circuit_t *circ, uint64_t uid, uint8_t hopnum);

int plugin_process_plug_cell(circuit_t *circ, const uint8_t *cell_payload,
    uint16_t cell_payload_len);

plugin_entry_point_t *plugin_get_entry_point_by_entry_name(plugin_t *plugin, char *entry_name);
/**
 * Execute the loaded and compiled plugin code
 */
uint64_t plugin_run(plugin_entry_point_t *ep, void *args, size_t size);

entry_point_map_t *plugin_get(entry_point_map_t *key);
void plugin_map_entrypoint_remove(plugin_entry_point_t *ep);

/** Cleanup any connection plugin with given uid */
void plugin_cleanup_conn(circuit_t *circ, uint64_t uid);

/**********************************PLUGIN API***************************/

/**
 * API accessible from plugins
 *
 */

uint64_t get(int key, int arglen, ...);

void set(int key, int arglen, ...);

/** used to call function with more than 5 parameters
 * 
 * -- UBPF limitation-- */
int call_host_func(int key, int size, ...);

/** private definitions */

#ifdef PLUGIN_PRIVATE
STATIC int plugins_compare_by_uid_(const void **a_, const void **b_);
#endif /* defined(PLUGIN_PRIVATE) */
#endif
