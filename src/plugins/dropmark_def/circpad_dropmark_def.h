#ifndef TOR_DROPMARKDEF_H
#define TOR_DROPMARKDEF_H

/**
 * New event types used by this plugin. This values MUST not conflict with
 * another plugin's values or the host ones.
 *
 * XXX How to avoid clashes? We may ask the host to choose values as part of a
 * initialization function and keep an increasing integer on the host to
 * allocate event numbers, state  numbers, etc..
 */

/**
 * this struct would be shared between entry_points of the dropmark plguin
 *
 * Since we do not have a linker for the different compiled object, sharing
 * global variables, e.g., event numbers, need to be referred in a shared
 * context
 */

#define CIRCPAD_EVENT_SHOULD_SIGPLUGIN_ACTIVATE 1
#define CIRCPAD_EVENT_SHOULD_SIGPLUGIN_BE_SILENT 2

#define CIRCPAD_STATE_SILENCE 3

typedef struct circpad_dropmark_t {

  int CIRCPAD_EVENT_SIGPLUGIN_ACTIVATE;
  int CIRCPAD_EVENT_SIGPLUGIN_BE_SILENT;
  int CIRCPAD_EVENT_SIGPLUGIN_CLOSE;

} circpad_dropmark_t;

#define PLUGIN_NUM_EVENTS 3
#define RELAY_COMMAND_CIRCPAD_SIGPLUGIN 43

#endif
