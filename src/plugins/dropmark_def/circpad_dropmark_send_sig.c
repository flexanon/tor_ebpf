#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "plugins/dropmark_def/circpad_dropmark_def.h"
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
circpad_plugin_transition_check(const circpad_plugin_transition_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->command == CIRCPAD_COMMAND_SIGPLUGIN))
    return "Integer out of bounds";
  if (! (obj->signal_type == CIRCPAD_SIGPLUGIN_ACTIVATE || obj->signal_type == CIRCPAD_SIGPLUGIN_BE_SILENT || obj->signal_type == CIRCPAD_SIGPLUGIN_CLOSE))
    return "Integer out of bounds";
  return NULL;
}

static __attribute__((always_inline)) ssize_t
circpad_plugin_transition_encoded_len(const circpad_plugin_transition_t *obj)
{
  ssize_t result = 0;

  if (NULL != circpad_plugin_transition_check(obj))
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
circpad_plugin_transition_encode(uint8_t *output, const size_t avail, const circpad_plugin_transition_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = circpad_plugin_transition_encoded_len(obj);
#endif

  if (NULL != (msg = circpad_plugin_transition_check(obj)))
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
  trunnel_set_uint32(ptr, trunnel_htonl(obj->machine_ctr));
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
  (void)msg;
  result = -1;
  goto fail;
 fail:
  assert(result < 0);
  return result;
}

uint64_t circpad_dropmark_def_send_activate_sig(conn_edge_plugin_args_t *args) {

  circuit_t *circ = get(CONNEDGE_CIRCUIT_T, args);
  /* get padding machine  -- we may have multiple padding machines per circuit,
   * but only one is the dropmark machine :)*/
  cell_t cell;
  my_plugin_memset(&cell, 0, sizeof(cell));
  cell.command = CELL_RELAY;
  circpad_plugin_transition_t activate_sig;
  my_plugin_memset(&activate_sig, 0, sizeof(activate_sig));
  activate_sig.command = CIRCPAD_COMMAND_SIGPLUGIN;
  activate_sig.signal_type = CIRCPAD_SIGPLUGIN_ACTIVATE;
  activate_sig.machine_ctr = ?;
  circpad_plugin_transition_encode(cell.payload, CELL_PAYLOAD_SIZE, &activate_sig);

  return 0;
}
