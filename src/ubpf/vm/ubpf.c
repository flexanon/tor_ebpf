#include "ubpf/vm/inc/ubpf.h"
#include <inttypes.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <elf.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include "core/or/plugin.h"
#include "core/or/circuitpadding.h"
#include "core/or/plugin_memory.h"
#include "core/or/relay.h"
#include "lib/log/log.h"
#include "ext/trunnel/trunnel-impl.h"

#define JIT true  /* putting to false show out of memory access */

/**
 * Depending on the plugin type
 *  - plugin_dev_t
 *  - plugin_user_t
 * call appropriate register_[dev/user]_functions
 */

static void 
register_functions(plugin_t *plugin)
{
  (void) plugin;
}

/** todo check error */ 
static void
register_dev_functions(struct ubpf_vm *vm)
{
  /* We can register 64 external functions ...*/
  // todo check to increase
  unsigned int idx = 0;
  /* specific API related */
  tor_assert(ubpf_register(vm, idx++, "invoke_plugin_operation_or_default", invoke_plugin_operation_or_default) != -1);
  tor_assert(ubpf_register(vm, idx++, "get", get) != -1);
  tor_assert(ubpf_register(vm, idx++, "set", set) != -1);
  tor_assert(ubpf_register(vm, idx++, "call_host_func", call_host_func) != -1);

  tor_assert(ubpf_register(vm, idx++, "my_ntohl", my_ntohl) != -1);
  // we could map my_htonl to my_ntohl directly instead
  tor_assert(ubpf_register(vm, idx++, "my_htonl", my_htonl) != -1);

  /** memory */
  tor_assert(ubpf_register(vm, idx++, "my_plugin_malloc", my_plugin_malloc) != -1);
  tor_assert(ubpf_register(vm, idx++, "my_plugin_free", my_plugin_free) != -1);
  tor_assert(ubpf_register(vm, idx++, "my_plugin_memset", my_plugin_memset) != -1);
  tor_assert(ubpf_register(vm, idx++, "my_plugin_memcpy", my_plugin_memcpy) != -1);
  /** logging stuff */
  tor_assert(ubpf_register(vm, idx++, "log_fn_", log_fn_) != -1);
  /** circpad */
  tor_assert(ubpf_register(vm, idx++, "circpad_machine_spec_transition", circpad_machine_spec_transition) != -1);
  /** trunnel */
  tor_assert(ubpf_register(vm, idx++, "trunnel_set_uint8", trunnel_set_uint8) != -1);
  tor_assert(ubpf_register(vm, idx++, "trunnel_set_uint16", trunnel_set_uint16) != -1);
  tor_assert(ubpf_register(vm, idx++, "trunnel_set_uint32", trunnel_set_uint32) != -1);

  tor_assert(ubpf_register(vm, idx++, "trunnel_get_uint8", trunnel_get_uint8) != -1);
  tor_assert(ubpf_register(vm, idx++, "trunnel_get_uint16", trunnel_get_uint16) != -1);
  tor_assert(ubpf_register(vm, idx++, "trunnel_get_uint32", trunnel_get_uint32) != -1);

  /** some tools */
  tor_assert(ubpf_register(vm, idx++, "strcmp", strcmp) != -1);

}

  static void
register_user_functions(struct ubpf_vm *vm)
{
  (void)vm;
}

static void *readfile(const char *path, size_t maxlen, size_t *len)
{
  FILE *file;
  if (!strcmp(path, "-")) {
    file = fdopen(STDIN_FILENO, "r");
  } else {
    file = fopen(path, "r");
  }

  if (!file) {
    log_debug(LD_PLUGIN, "Failed to open %s: %s\n", path, strerror(errno));
    return NULL;
  }

  char *data = calloc(maxlen, 1);
  size_t offset = 0;
  size_t rv;
  while ((rv = fread(data+offset, 1, maxlen-offset, file)) > 0) {
    offset += rv;
  }
  int ret;
  if ((ret = ferror(file))) {
    log_debug(LD_PLUGIN, "Failed to read %s: %s\n", path, strerror(errno));
    fclose(file);
    tor_free(data);
    return NULL;
  }

  /*if ((ret = feof(file)) == 0) {*/
    /*log_debug(LD_PLUGIN, "Failed to read %s because it is too large (max %u bytes) and current offset: %lu",*/
        /*path, (unsigned)maxlen, offset);*/
    /*fclose(file);*/
    /*tor_free(data);*/
    /*return NULL;*/
  /*}*/

  fclose(file);
  if (len) {
    *len = offset;
  }
  return data;
}

int load_elf(void *code, size_t code_len, plugin_t *plugin, plugin_entry_point_t *entry_point) {

  entry_point->vm = ubpf_create();
  if (!entry_point->vm) {
    log_debug(LD_PLUGIN, "Failed to create VM\n");
    return -1;
  }
  /**
   * TODO
   * Allow to set different register_function per entry_point!
   */
  register_dev_functions(entry_point->vm);

  bool elf = code_len >= SELFMAG && !memcmp(code, ELFMAG, SELFMAG);

  char *errmsg;
  int rv;
  if (elf) {
    rv = ubpf_load_elf(entry_point->vm, code, code_len, &errmsg, (uint64_t) plugin->memory, plugin->memory_size);
  } else {
    rv = ubpf_load(entry_point->vm, code, code_len, &errmsg, (uint64_t) plugin->memory, plugin->memory_size);
  }

  if (rv < 0) {
    log_debug(LD_PLUGIN, "Failed to load code: %s", errmsg);
    tor_free(errmsg);
    ubpf_destroy(entry_point->vm);
    return -1;
  }
  entry_point->fn = ubpf_compile(entry_point->vm, &errmsg);
  if (entry_point->fn == NULL) {
    log_debug(LD_PLUGIN, "Failed to compile: %s", errmsg);
    tor_free(errmsg);
    ubpf_destroy(entry_point->vm);
    return -1;
  }

  tor_free(errmsg);

  return 0;
}

MOCK_IMPL(int, load_elf_file, (const char *code_filename, plugin_t *plugin, plugin_entry_point_t *entry_point)) {
  size_t code_len;
  void *code = readfile(code_filename, 1024*1024, &code_len);
  if (code == NULL) {
    return -1;
  }

  int ret = load_elf(code, code_len, plugin, entry_point);
  tor_free(code);
  return ret;
}

void release_elf(plugin_entry_point_t *entry) {
  if (!entry)
    return;
  if (entry->vm) {
    ubpf_destroy(entry->vm);
    entry->vm = NULL;
    entry->fn = 0;
  }
}

uint64_t exec_loaded_code(plugin_entry_point_t *entry_point, void *mem, size_t mem_len) {
  uint64_t ret;
  if (entry_point->vm == NULL) {
    return 1;
  }
  if (entry_point->fn == NULL && JIT) {
    return 1;
  }
  if (JIT) {
    /* JIT */
    ret = entry_point->fn(mem, mem_len);
  } else {
    /* Interpreted */
    ret = ubpf_exec(entry_point->vm, mem, mem_len);
  }

  /* printf("0x%"PRIx64"\n", ret); */

  return ret;
}
