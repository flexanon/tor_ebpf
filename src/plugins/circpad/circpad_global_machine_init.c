#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "ubpf/vm/plugin_memory.h"

static __attribute__((always_inline)) void register_client_machine(plugin_t *plugin, smartlist_t *client_machines) {

  circpad_machine_spec_t *client_machine = my_plugin_malloc(plugin, sizeof(circpad_machine_spec_t));
  memset(client_machine, 0, sizeof(circpad_machine_spec_t));

  client_machine->name = "client_wf_ape";
  client_machine->is_origin_side = 1; // client-side

  /** Pad to/from the middle relay, only when the circuit has streams, and only
  * for general purpose circuits (typical for web browsing)
  */
  client_machine->target_hopnum = 2;
  client_machine->conditions.min_hops = 2;
  client_machine->conditions.apply_state_mask = CIRCPAD_CIRC_STREAMS;
  client_machine->conditions.apply_purpose_mask =
    (circpad_purpose_mask_t) call_host_func(CIRCPAD_CIRC_PURPOSE_TO_MASK, 1, (uint32_t) CIRCUIT_PURPOSE_C_GENERAL);

  // limits to help guard against excessive padding
  client_machine->allowed_padding_count = 1;
  client_machine->max_padding_percent = 1;

  // one state to start with: START (-> END, never takes a slot in states)
  circpad_machine_states_init(client_machine, 1);
  call_host_func(CIRCPAD_MACHINE_STATES_INIT, 2, client_machine, 1);
  client_machine->states[CIRCPAD_STATE_START].
    next_state[CIRCPAD_EVENT_NONPADDING_SENT] =
    CIRCPAD_STATE_END;

  client_machine->machine_num = get(CIRCPAD_MACHINE_LIST_SIZE, client_machines);

  call_host_func(CIRCPAD_REGISTER_PADDING_MACHINE, 2, client_machine, client_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
           "Registered client WF APE padding machine (%u)",
           client_machine->machine_num);
}

static __attribute__((always_inline)) void register_relay_machine(plugin_t *plugin, smartlist_t *relay_machines) {
  circpad_machine_spec_t *relay_machine = my_plugin_malloc(plugin, sizeof(circpad_machine_spec_t));
  memset(relay_machine, 0, sizeof(circpad_machine_spec_t));
  relay_machine->name = "relay_wf_ape";
  relay_machine->is_origin_side = 0; // relay-side

  // Pad to/from the middle relay, only when the circuit has streams
  relay_machine->target_hopnum = 2;
  relay_machine->conditions.min_hops = 2;
  relay_machine->conditions.apply_state_mask = CIRCPAD_CIRC_STREAMS;

  // limits to help guard against excessive padding
  relay_machine->allowed_padding_count = 1;
  relay_machine->max_padding_percent = 1;

  // one state to start with: START (-> END, never takes a slot in states)
  call_host_func(CIRCPAD_MACHINE_STATES_INIT, 2, relay_machine, 1);
  relay_machine->states[CIRCPAD_STATE_START].
    next_state[CIRCPAD_EVENT_NONPADDING_SENT] =
    CIRCPAD_STATE_END;
  /** dereference relay_machines */
  relay_machine->machine_num = get(CIRCPAD_MACHINE_LIST_SIZE, relay_machines);
  // register the machine
  call_host_func(CIRCPAD_REGISTER_PADDING_MACHINE, 2, relay_machine, relay_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Registered relay WF APE padding machine (%u)", relay_machine->machine_num);
}

uint64_t circpad_global_machine_init(circpad_plugin_args_t *args) {
  smartlist_t *client_machines = args->origin_padding_machines;
  smartlist_t *relay_machines = args->relay_padding_machines;

  register_relay_machine(args->plugin, relay_machines);
  register_client_machine(args->plugin, client_machines);
  return 0;
}
