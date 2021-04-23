#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "ubpf/vm/plugin_memory.h"


static __attribute__((always_inline)) void
plugin_circpad_machine_states_init(plugin_t *plugin, circpad_machine_spec_t *machine,
      circpad_statenum_t num_states) {
  if (BUG(num_states > CIRCPAD_MAX_MACHINE_STATES)) {
    num_states = CIRCPAD_MAX_MACHINE_STATES;
  }

  machine->num_states = num_states;
  machine->states = my_plugin_malloc(plugin, sizeof(circpad_state_t)*num_states);

  /* Initialize the default next state for all events to
   * "ignore" -- if events aren't specified, they are ignored. */
  for (circpad_statenum_t s = 0; s < num_states; s++) {
    for (int e = 0; e < CIRCPAD_NUM_EVENTS; e++) {
      machine->states[s].next_state[e] = CIRCPAD_STATE_IGNORE;
    }
  }
}


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
  /**
   * States are malloc'ed outside of our sandbox if we call
   * circpad_machine_states_init.
   * There are two solutions. Either we rewrite a function that does the
   * initialization here,  or we call circpad_machine_states_init but we have to
   * use a set() and get() to manage the machine states, but we must not free
   * the states in the sandbox!
   *
   * Let's do the first solution for simplicity :)
   **/
  /*call_host_func(CIRCPAD_MACHINE_STATES_INIT, 2, client_machine, 1);*/
  plugin_circpad_machine_states_init(plugin, client_machine, 1);
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

  /*call_host_func(CIRCPAD_MACHINE_STATES_INIT, 2, relay_machine, 1);*/
  plugin_circpad_machine_states_init(plugin, relay_machine, 1);
  // one state to start with: START (-> END, never takes a slot in states)

  relay_machine->states[CIRCPAD_STATE_START].
    next_state[CIRCPAD_EVENT_NONPADDING_SENT] =
    CIRCPAD_STATE_END;
  /** dereference relay_machines */
  // one state to start with: START (-> END, never takes a slot in states)
  relay_machine->machine_num = get(CIRCPAD_MACHINE_LIST_SIZE, relay_machines);
  // register the machine
  // one state to start with: START (-> END, never takes a slot in states)
  call_host_func(CIRCPAD_REGISTER_PADDING_MACHINE, 2, relay_machine, relay_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Registered relay WF APE padding machine (%u)", relay_machine->machine_num);
}

uint64_t circpad_global_machine_init(circpad_plugin_args_t *args) {
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Entering");
  smartlist_t *client_machines = (smartlist_t*) get(CIRCPAD_CLIENT_MACHINES_SL, args);
  smartlist_t *relay_machines = (smartlist_t*) get(CIRCPAD_RELAY_MACHINES_SL, args);
  plugin_t *plugin = (plugin_t *) get(CIRCPAD_PLUGIN_T, args);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Calling register_relay_machine");
  register_relay_machine(plugin, relay_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Calling register_client_machine");
  register_client_machine(plugin, client_machines);
  return 0;
}
