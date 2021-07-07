#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "plugins/dropmark_def/circpad_dropmark_def.h"
#include "ubpf/vm/plugin_memory.h"

static int num_events = CIRCPAD_NUM_EVENTS + PLUGIN_NUM_EVENTS;

/**
 * The plugin has ownership of the machines's memory. States should be malloc'ed
 * by the plugin to easily access them as well.
 *
 * Another way to solve the problem would be hijack the
 * circpad_machine_states_init and then call
 * invoke_plugin_operation_or_default() from any other plugin to initiate the
 * state within the sandboxed memory, but it way more cumbersome than simply
 * re-writting the function in the only file where it is needed (here). If it is
 * eventually needed at several places, writing a hijack plugin would be the
 * way to go.
 */

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
    machine->states[s].next_state = my_plugin_malloc(plugin, num_events*sizeof(circpad_statenum_t));
    machine->states[s].circpad_num_events = num_events;
    for (int e = 0; e < num_events; e++) {
      machine->states[s].next_state[e] = CIRCPAD_STATE_IGNORE;
    }
  }
}

/**
 * Let's try to create a machine tasked to send random bursts of paddings
 * uniformly distributed within a period of expected silence.
 *
 * The period of expected silence should be notified by the client to make the
 * machine transitioning to this state of active padding. When the period ends,
 * again notified by the client, the machine should be itself silent.
 */

static __attribute__((always_inline)) void register_relay_machine(plugin_t
    *plugin, circpad_dropmark_t *ctx, smartlist_t *relay_machines) {

  circpad_machine_spec_t *relay_machine = my_plugin_malloc(plugin, sizeof(circpad_machine_spec_t));
  relay_machine->name = "relay_dropmark_def";
  // let's just use plugin_machine_spec as a placeholder for a unique name
  relay_machine->plugin_machine_spec = (void *) relay_machine->name;

  relay_machine->conditions.apply_state_mask = CIRCPAD_CIRC_OPENED;

  relay_machine->is_origin_side = 0;

  /** TODO reconfigure the number of events and add the new events */

  plugin_circpad_machine_states_init(plugin, relay_machine, 4);


  /** We make send rand(1, 42) cells at uniformly random intervals  */
  relay_machine->states[CIRCPAD_STATE_BURST].
    length_dist.type = CIRCPAD_DIST_UNIFORM;
  relay_machine->states[CIRCPAD_STATE_BURST].
    length_dist.param1 = 3;
  relay_machine->states[CIRCPAD_STATE_BURST].
    length_dist.param2 = 9;

  /* define the histogram  -- this should make the state chooses a timer between [1,
   * 100ms] uniformly before sending the padding, then circle back to this state*/
  relay_machine->states[CIRCPAD_STATE_BURST].
    histogram_len = 2;
  relay_machine->states[CIRCPAD_STATE_BURST].
    histogram_edges[0] = 1; // 1µs
  relay_machine->states[CIRCPAD_STATE_BURST].
    histogram_edges[1] = 1000; //1ms

  relay_machine->states[CIRCPAD_STATE_BURST].
    histogram[0] = 1000;
  
  relay_machine->states[CIRCPAD_STATE_BURST].
    histogram_total_tokens = relay_machine->states[CIRCPAD_STATE_BURST].
    histogram[0];
  
  /** Send 1 padding at the end of the GAP time */
  relay_machine->states[CIRCPAD_STATE_GAP].
    length_dist.type = CIRCPAD_DIST_UNIFORM;
  relay_machine->states[CIRCPAD_STATE_GAP].
    length_dist.param1 = 1;
  relay_machine->states[CIRCPAD_STATE_GAP].
    length_dist.param2 = 2;

  relay_machine->states[CIRCPAD_STATE_GAP].
    histogram_len = 2;
  relay_machine->states[CIRCPAD_STATE_GAP].
    histogram_edges[0] = 1000; // 1ms
  relay_machine->states[CIRCPAD_STATE_GAP].
    histogram_edges[1] = 100000; // 100ms

  relay_machine->states[CIRCPAD_STATE_GAP].
    histogram[0] = 1000;
  relay_machine->states[CIRCPAD_STATE_GAP].
    histogram_total_tokens = relay_machine->states[CIRCPAD_STATE_GAP].
    histogram[0];

  /** Activate the machine as soon as a non-padding cell is received */
  relay_machine->states[CIRCPAD_STATE_START].
    next_state[CIRCPAD_EVENT_NONPADDING_SENT] =
    CIRCPAD_STATE_BURST;

  /* Transition from the start state to burst state when the client
   * tells us to do so */

  relay_machine->states[CIRCPAD_STATE_START].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE] =
    CIRCPAD_STATE_BURST;

  /** schedule more padding until state_length is 0 */
  relay_machine->states[CIRCPAD_STATE_BURST].
    next_state[CIRCPAD_EVENT_PADDING_SENT] =
    CIRCPAD_STATE_BURST;
  
  /* transition to the GAP state when all the padding cells are sent -- we wait again some random
   * interval and then transition to the BURST state again */
  relay_machine->states[CIRCPAD_STATE_BURST].
    next_state[CIRCPAD_EVENT_LENGTH_COUNT] =
    CIRCPAD_STATE_GAP;


  /* We try to schedule padding after some random time, but get the infinity
   * bin. So we don't schedule anything and goes to the burst state. This
   * mechanic implements a random gap */
  relay_machine->states[CIRCPAD_STATE_GAP].
    next_state[CIRCPAD_EVENT_PADDING_SENT] =
    CIRCPAD_STATE_BURST;

  /* goes silent mode if the client tells to do so */
  relay_machine->states[CIRCPAD_STATE_GAP].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT] =
    CIRCPAD_STATE_SILENCE;

  /* When the client tells us to be silent, we move to the gap state in which
   * nothing should happen */
  relay_machine->states[CIRCPAD_STATE_BURST].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT] =
    CIRCPAD_STATE_SILENCE;

  relay_machine->states[CIRCPAD_STATE_SILENCE].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE] =
    CIRCPAD_STATE_BURST;

  relay_machine->machine_num = get(CIRCPAD_MACHINE_LIST_SIZE, 1, relay_machines);
  call_host_func(CIRCPAD_REGISTER_PADDING_MACHINE, 2, relay_machine, relay_machines);

  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Registered dropmark defense padding machine (%u)", relay_machine->machine_num);

}

static __attribute__((always_inline)) void register_client_machine(plugin_t
    *plugin, circpad_dropmark_t *ctx, smartlist_t *client_machines) {
  circpad_machine_spec_t *client_machine = my_plugin_malloc(plugin, sizeof(circpad_machine_spec_t));

  client_machine->name = "client_dropmark_def";
  client_machine->target_hopnum = 2;

  client_machine->conditions.min_hops = 3;
  client_machine->is_origin_side = 1;

  client_machine->conditions.apply_state_mask = CIRCPAD_CIRC_STREAMS;

  client_machine->conditions.apply_purpose_mask =
    (circpad_purpose_mask_t) call_host_func(CIRCPAD_CIRC_PURPOSE_TO_MASK, 1, (uint8_t) CIRCUIT_PURPOSE_C_GENERAL);


  /* This event should be triggered when the client sends a RELAY_BEGIN
   * It tells the middle relay to stop the padding */
  plugin_circpad_machine_states_init(plugin, client_machine, 2);

  client_machine->states[CIRCPAD_STATE_START].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT] =
    CIRCPAD_STATE_START;
  /**
   * This event should trigger a new message to the middle relay to activate
   * padding
   **/
  client_machine->states[CIRCPAD_STATE_START].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE] =
    CIRCPAD_STATE_START;

  client_machine->states[CIRCPAD_STATE_START].
    next_state[ctx->CIRCPAD_EVENT_SIGPLUGIN_CLOSE] =
    CIRCPAD_STATE_END;

  client_machine->should_negotiate_end = 1;

  client_machine->machine_num = (int) get(CIRCPAD_MACHINE_LIST_SIZE, 1, client_machines);

  call_host_func(CIRCPAD_REGISTER_PADDING_MACHINE, 2, client_machine, client_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Registered dropmark defense client padding machine (%u)", client_machine->machine_num);
}

uint64_t circpad_dropmark_defense(circpad_plugin_args_t *args) {
  //register events
  smartlist_t *client_machines = (smartlist_t*) get(CIRCPAD_CLIENT_MACHINES_SL, 1, args);
  smartlist_t *relay_machines = (smartlist_t*) get(CIRCPAD_RELAY_MACHINES_SL, 1, args);
  plugin_t *plugin = (plugin_t *) get(CIRCPAD_ARG_PLUGIN_T, 1, args);
  circpad_dropmark_t *ctx = my_plugin_malloc(plugin, sizeof(*ctx));
  ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  ctx->CIRCPAD_EVENT_SIGPLUGIN_CLOSE = (int) get(CIRCPAD_NEW_EVENTNUM, 0, NULL);
  set(CIRCPAD_PLUGIN_CTX, 2, args, ctx);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Registering relay machine");
  /** Clients can register a relay machine -- they're never used anyway */
  register_relay_machine(plugin, ctx, relay_machines);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Relay machine registered");
  int ORPort_is_set = (int) get(OPTIONS_ORPORT_SET, 0);
  /** avoif relays to register the client machine and get paddings with every
   * circuit they create */
  if (!ORPort_is_set) {
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Registering client machine");
    register_client_machine(plugin, ctx, client_machines);
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__, "Client machine registered");
  }
  return 0;
}
