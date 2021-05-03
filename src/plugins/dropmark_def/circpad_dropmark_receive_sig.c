#include "core/or/or.h"
#include "core/or/circuitlist.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin.h"
#include "core/or/plugin_helper.h"
#include "plugins/dropmark_def/circpad_dropmark_def.h"
#include "ubpf/vm/plugin_memory.h"
#include "ext/trunnel/trunnel-impl.h"
#include <assert.h>
#include <stdlib.h>


/**
 * Static function to parse a CIRCPAD_COMMAND_SIGPLUGIN signal -- put the parsed
 * data into a circpad_plugin_transition_t object
 *
 * This function has been generated and modified from the trunnel tool
 */

static __attribute__((always_inline)) ssize_t
circpad_plugin_transition_parse_into(circpad_plugin_transition_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 command IN [CIRCPAD_COMMAND_SIGPLUGIN] */
  CHECK_REMAINING(1, truncated);
  obj->command = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->command == CIRCPAD_COMMAND_SIGPLUGIN))
    goto fail;

  /* Parse u8 signal_type IN [CIRCPAD_SIGPLUGIN_ACTIVATE, CIRCPAD_SIGPLUGIN_BE_SILENT, CIRCPAD_SIGPLUGIN_CLOSE] */
  CHECK_REMAINING(1, truncated);
  obj->signal_type = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->signal_type == CIRCPAD_SIGPLUGIN_ACTIVATE || obj->signal_type == CIRCPAD_SIGPLUGIN_BE_SILENT || obj->signal_type == CIRCPAD_SIGPLUGIN_CLOSE))
    goto fail;

  /* Parse u32 machine_ctr */
  CHECK_REMAINING(4, truncated);
  obj->machine_ctr = my_ntohl(trunnel_get_uint32(ptr));
  remaining -= 4; ptr += 4;
  assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

/**
 * This should be hooked in the relay protocol -- we received a message that
 * this plugin can understand.
 */

uint64_t circpad_dropmark_def_receive_sig(relay_process_edge_t *pedge) {

  cell_t *cell = (cell_t *) get(RELAY_CELL_T, pedge);
  cell_t mycell;
  char *payload = get(RELAY_CELL_PAYLOAD, cell);
  my_plugin_memset(&mycell, 0, sizeof(mycell));
  // get accessible content of cell to be parsed
  my_plugin_memcpy(&mycell, cell, sizeof(mycell));
  mycell.payload = my_plugin_malloc(sizeof(char)*CELL_PAYLOAD_SIZE);
  my_plugin_memcpy(mycell.payload, payload, CELL_PAYLOAD_SIZE);

  circpad_plugin_transition_t signal_transition;
  /** parse the cell and fill in the structure */
  circpad_parse_plugin_signal(&signal_transition,
      mycell->payload+RELAY_HEADER_SIZE, CELL_PAYLOAD_SIZE-RELAY_HEADER_SIZE);
  // get the machine runtime

  circpad_machine_spec_transition(, signal_transition.signal_type);
  my_plugin_free(mycell.payload);
  return 0;
  /** What event do we have here? -- just notify the transition */
  //XXX todo
}
