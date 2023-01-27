#include "core/or/or.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "lib/log/ratelim.h"
#include "circpad_drop_def_conn_based.h"

/**
 * This function replaces the Tor core function
 * "circpad_send_padding_for_callback"
 *
 *
 */

uint64_t circpad_dropmark_send_padding_for_callback(circpad_plugin_args_t *args) {
  circuit_t *circ = (circuit_t *) get(CIRCPAD_ARG_CIRCUIT_T, 1, args);
  circpad_machine_runtime_t *mi = (circpad_machine_runtime_t *) get(CIRCPAD_ARG_MACHINE_RUNTIME, 1, args);
  circpad_machine_spec_t *ms = (circpad_machine_spec_t *) get(CIRCPAD_ARG_MACHINE_SPEC_T, 1, args);
  set(CIRCPAD_MACHINE_RUNTIME_PADDING_SCHEDULED_AT_USEC, 2, mi, (circpad_time_t) 0);
  set(CIRCPAD_MACHINE_RUNTIME_IS_PADDING_TIMER_SCHEDULED, 2, mi, (uint32_t) 0);
  int marked_for_close = (int) get(CIRCUIT_MARKED_FOR_CLOSE, 1, circ);
  circpad_statenum_t state = (circpad_statenum_t) get(CIRCPAD_MACHINE_RUNTIME_STATE, 1, mi);
  if (marked_for_close) {
    // todo log
    return CIRCPAD_STATE_CHANGED;
  }

  int is_origin_circ =  (int) get(UTIL_CIRCUIT_IS_ORIGIN, 1, circ);
  char *name = (char *) get(CIRCPAD_MACHINE_SPEC_NAME, 1, ms);
  int is_dropmark_machine = 0;
  uint64_t state_length = get(CIRCPAD_MACHINE_RUNTIME_STATELENGTH, 1, mi);
  if (name && (!strcmp(name, "client_dropmark_def") || !strcmp(name, "relay_dropmark_def")))
    is_dropmark_machine = 1;
  else {
    // behaves normally -- 1 padding at a time.
    state_length = 1;
  }

  circpad_connbased_dropmark_t* ctx = (circpad_connbased_dropmark_t*) get(UTIL_CONN_CTX, 1, circ);
  if (!ctx) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Ctx is NULL?");
    return -1;
  }
  uint32_t n_circ_id = get(CIRCUIT_N_CIRC_ID, 1, circ);
  if (is_origin_circ) {
    uint8_t target_hop = get(CIRCPAD_MACHINE_SPEC_TARGET_HOP, 1, ms);
    for (int i = 0; i < state_length; i++) {
      call_host_func(CIRCPAD_MACHINE_COUNT_PADDING_SENT, 1, mi);
      call_host_func(CIRCPAD_SEND_COMMAND_DROP_TO_HOP, 2, circ, (uint8_t) target_hop);
    }
  }
  else {
    channel_t *chan = (channel_t *) get(RELAY_ARG_CIRCUIT_CHAN_T, 1, circ);
    int circpad_max_circ_queued_cells = (int) get(CIRCPAD_MAX_CIRC_QUEUED_CELLS, 0);
    plugin_t *plugin = (plugin_t *) get(CIRCPAD_ARG_PLUGIN_T, 1, args);
    for (int i = 0; i < state_length; i++) {
      int n_cells_queued = (int) get(CIRCUIT_P_CHAN_QUEUED_CELLS, 1, circ);
      if (n_cells_queued < circpad_max_circ_queued_cells) {
        call_host_func(CIRCPAD_MACHINE_COUNT_PADDING_SENT, 1, mi);
        if (!fifo_is_empty(ctx->cell_queue)) {
          log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "flushing a delayed cell");
          cell_t *cell = NULL;
          queue_ret_t ret = queue_pop(ctx->cell_queue, &cell);
          if (ret != OK) {
            log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
                "Queue_pop somehow didn't return OK");
            return -1;
          }
          log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
              "Plugin: Flushing a delayed cell!");
          call_host_func(RELAY_APPEND_CELL_TO_CIRCUIT_QUEUE, 3, circ, chan, cell);
          my_plugin_free(plugin, cell);
        }
        else {
          call_host_func(RELAY_SEND_COMMAND_FROM_EDGE, 3, circ, (uint32_t)
            RELAY_COMMAND_DROP, NULL);
        }
      }
      else {
        static ratelim_t cell_lim = RATELIM_INIT(600);
        break;
        /*call_host_func(LOG_FN_RATELIM, 3, &cell_lim, "Too many cells (%d) in circ queue to send padding.",*/
            /*n_cells_queued);*/
      }
    }
  }
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Callback: Sending to origin circuit %u",
      n_circ_id);
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Sending %lu drop cells", state_length);

  call_host_func(CIRCPAD_CELL_EVENT_PADDING_SENT, 1, circ);
  
  if (mi != NULL) {
    if (state != (circpad_statenum_t) get(CIRCPAD_MACHINE_RUNTIME_STATE, 1, mi))
      return CIRCPAD_STATE_CHANGED;
    else
      return (circpad_decision_t) call_host_func(CIRCPAD_CHECK_MACHINE_TOKEN_SUPPLY, 1, mi);
  }
  else {
    return CIRCPAD_STATE_CHANGED;
  }
}
