#include "core/or/plugin.h"
/* things that can be defined in a .h  and included here */
#include "hello_world_features.h"
/* My plugin main entry point */
uint64_t hello_world(void *args) {
  log_fn_(LOG_DEBUG, LD_PLUGIN, __FUNCTION__,
      "I have become sentient. Run.");
  return 0;
}

