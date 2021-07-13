/* circpad_dropmark_plugin.h -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 *
 * Modified after generation
 */
#ifndef TRUNNEL_CIRCPAD_DROPMARK_PLUGIN_H
#define TRUNNEL_CIRCPAD_DROPMARK_PLUGIN_H

#include <stdint.h>
#include "ext/trunnel/trunnel.h"

#if defined(__COVERITY__) || defined(__clang_analyzer__)

int circpadnegotiation_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || circpadnegotiation_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#define CIRCPAD_COMMAND_SIGPLUGIN 1
#define CIRCPAD_SIGPLUGIN_ACTIVATE 3
#define CIRCPAD_SIGPLUGIN_BE_SILENT 4
#define CIRCPAD_SIGPLUGIN_CLOSE 5
struct circpad_plugin_transition_st {
  uint8_t command;
  uint8_t signal_type;
  uint32_t machine_ctr;
  uint8_t trunnel_error_code_;
};
typedef struct circpad_plugin_transition_st circpad_plugin_transition_t;

#endif