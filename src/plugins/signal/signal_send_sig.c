#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/plugin.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin_helper.h"
#include "core/or/cell_st.h"
#include "plugins/signal/signal_def.h"
#include "plugins/signal/parsing/circpad_dropmark_plugin.h"
#include "ubpf/vm/plugin_memory.h"
#include "ext/trunnel/trunnel-impl.h"
#include "core/or/connection_edge.h"
#include <assert.h>
#include <stdlib.h>

/**
 * Static functions to package the CIRCPAD_COMMAND_SIGPLUGIN cell to an output
 * buffer -- These function have been generated then modified from the trunnel tools.
 */

static __attribute__((always_inline)) char *
circpad_plugin_transition_check(const circpad_plugin_transition_t *obj, circpad_dropmark_t *ctx)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->command == CIRCPAD_COMMAND_SIGPLUGIN))
    return "Integer out of bounds";
  if (! (obj->signal_type == ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE || obj->signal_type
        == ctx->CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT || obj->signal_type ==
        ctx->CIRCPAD_EVENT_SIGPLUGIN_CLOSE))
    return "Integer out of bounds";
  return NULL;
}

static __attribute__((always_inline)) ssize_t
circpad_plugin_transition_encoded_len(const circpad_plugin_transition_t *obj, circpad_dropmark_t *ctx)
{
  ssize_t result = 0;

  if (NULL != circpad_plugin_transition_check(obj, ctx))
     return -1;


  /* Length of u8 command IN [CIRCPAD_COMMAND_SIGPLUGIN] */
  result += 1;

  /* Length of u8 signal_type IN [CIRCPAD_SIGPLUGIN_ACTIVATE, CIRCPAD_SIGPLUGIN_BE_SILENT, CIRCPAD_SIGPLUGIN_CLOSE] */
  result += 1;

  /* Length of u32 machine_ctr */
  result += 4;
  return result;
}

static __attribute__((always_inline)) ssize_t
circpad_plugin_transition_encode(uint8_t *output, const size_t avail, const circpad_plugin_transition_t *obj,
    circpad_dropmark_t *ctx)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = circpad_plugin_transition_encoded_len(obj, ctx);
#endif

  if (NULL != (msg = circpad_plugin_transition_check(obj, ctx)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  assert(encoded_len >= 0);
#endif

  /* Encode u8 command IN [CIRCPAD_COMMAND_SIGPLUGIN] */
  assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->command));
  written += 1; ptr += 1;

  /* Encode u8 signal_type IN [CIRCPAD_SIGPLUGIN_ACTIVATE, CIRCPAD_SIGPLUGIN_BE_SILENT, CIRCPAD_SIGPLUGIN_CLOSE] */
  assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->signal_type));
  written += 1; ptr += 1;

  /* Encode u32 machine_ctr */
  assert(written <= avail);
  if (avail - written < 4)
    goto truncated;
  trunnel_set_uint32(ptr, my_htonl(obj->machine_ctr));
  written += 4; ptr += 4;


  assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    assert(encoded_len >= 0);
    assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Check failed, got error message (%s)", msg);
  result = -1;
  goto fail;
 fail:
  assert(result < 0);
  return result;
}

/**
 * Borrow circpad code for another signaling purpose (ugly ok, but fast)
 */

uint64_t circpad_dropmark_def_send_activate_sig(conn_edge_plugin_args_t *args) {
  circuit_t *circ = (circuit_t *) get(CONNEDGE_ARG_CIRCUIT_T, 1, args);
  plugin_t *plugin = (plugin_t *) get(CONNEDGE_ARG_PLUGIN_T, 1, args);
  circpad_dropmark_t *ctx = (circpad_dropmark_t *) get(CONNEDGE_PLUGIN_CTX, 1, args);
  
  // check whether the circuit is 3 hops

  int pathlen = (int) get(CIRCUIT_PATH_LEN, 1, circ);
  if (pathlen < 3) {
    log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
        "Looks like machine it isn't a 3 hop circuit");
    return PLUGIN_RUN_DEFAULT;
  }
  /** The bpf stack is only 512 bytes. We cannot stack alloc a cell :'
   *  We could however create a function on the host to package properly a cell
   *  from a given payload < 512 bytes that could be stack-allocated */
  cell_t *cell;
  cell = my_plugin_malloc(plugin, sizeof(*cell));
  my_plugin_memset(cell, 0, sizeof(cell));
  cell->command = CELL_RELAY;
  circpad_plugin_transition_t activate_sig;
  my_plugin_memset(&activate_sig, 0, sizeof(activate_sig));
  activate_sig.command = CIRCPAD_COMMAND_SIGPLUGIN;
  activate_sig.signal_type = ctx->CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE;
  // we don't care
  activate_sig.machine_ctr = 0;
  ssize_t len = circpad_plugin_transition_encode(cell->payload, CELL_PAYLOAD_SIZE, &activate_sig, ctx);
  if (len < 0) {
    log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__, "Some issue occured: %zd", len);
    return PLUGIN_RUN_DEFAULT;
  }
  log_fn_(LOG_INFO, LD_PLUGIN, __FUNCTION__,
      "Our signal len is %zd and sending now.", len);
  call_host_func(CIRCPAD_SEND_COMMAND_TO_GUARD, 3, circ,
      cell->payload, (ssize_t) len);
  my_plugin_free(plugin, cell);
  return 0;
}
