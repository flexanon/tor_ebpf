#include <ubpf.h>
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
#include "ubpf/vm/plugin_memory.h"
#include "core/or/relay.h"
#include "lib/log/log.h"

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
  /* We only have 64 values ... (so far) */

  /* specific API related */
  ubpf_register(vm, 0x00, "invoke_plugin_operation_or_default", invoke_plugin_operation_or_default);
  ubpf_register(vm, 0x01, "get", get);
  ubpf_register(vm, 0x02, "set", set);

  ubpf_register(vm, 0x03, "call_host_func", call_host_func);
  ubpf_register(vm, 0x04, "my_ntohl", my_ntohl);
  /** memory */
  ubpf_register(vm, 0x15, "my_plugin_malloc", my_plugin_malloc);
  ubpf_register(vm, 0x16, "my_plugin_free", my_plugin_free);
  /** logging stuff */
  ubpf_register(vm, 0x20, "log_fn_", log_fn_);
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

  if (file == NULL) {
    fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
    return NULL;
  }

  char *data = calloc(maxlen, 1);
  size_t offset = 0;
  size_t rv;
  while ((rv = fread(data+offset, 1, maxlen-offset, file)) > 0) {
    offset += rv;
  }

  if (ferror(file)) {
    fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
    fclose(file);
    free(data);
    return NULL;
  }

  if (!feof(file)) {
    fprintf(stderr, "Failed to read %s because it is too large (max %u bytes)\n",
        path, (unsigned)maxlen);
    fclose(file);
    free(data);
    return NULL;
  }

  fclose(file);
  if (len) {
    *len = offset;
  }
  return data;
}

plugin_t *load_elf(void *code, size_t code_len) {
  plugin_t *plugin = plugin_memory_init();
  if (!plugin) {
    return NULL;
  }

  plugin->vm = ubpf_create();
  if (!plugin->vm) {
    log_debug(LD_PLUGIN, "Failed to create VM\n");
    free(plugin);
    return NULL;
  }
  /**
   * TODO
   * Allow to set the register function as a 
   * parameter
   */
  register_dev_functions(plugin->vm);

  bool elf = code_len >= SELFMAG && !memcmp(code, ELFMAG, SELFMAG);

  char *errmsg;
  int rv;
  if (elf) {
    rv = ubpf_load_elf(plugin->vm, code, code_len, &errmsg);
  } else {
    rv = ubpf_load(plugin->vm, code, code_len, &errmsg);
  }

  if (rv < 0) {
    log_debug(LD_PLUGIN, "Failed to load code: %s\n", errmsg);
    free(errmsg);
    ubpf_destroy(plugin->vm);
    free(plugin);
    return NULL;
  }
  plugin->fn = ubpf_compile(plugin->vm, &errmsg);
  if (plugin->fn == NULL) {
    log_debug(LD_PLUGIN, "Failed to compile: %s\n", errmsg);
    free(errmsg);
    ubpf_destroy(plugin->vm);
    free(plugin);
    return NULL;
  }

  free(errmsg);

  return plugin;
}

plugin_t *load_elf_file(const char *code_filename) {
  size_t code_len;
  void *code = readfile(code_filename, 1024*1024, &code_len);
  if (code == NULL) {
    return NULL;
  }

  plugin_t *ret = load_elf(code, code_len);
  free(code);
  return ret;
}

int release_elf(plugin_t *plugin) {
  if (plugin->vm != NULL) {
    ubpf_destroy(plugin->vm);
    plugin->vm = NULL;
    plugin->fn = 0;
    free(plugin);
  }
  return 0;
}

uint64_t exec_loaded_code(plugin_t *plugin, void *mem, size_t mem_len) {
  uint64_t ret;
  if (plugin->vm == NULL) {
    return 1;
  }
  if (plugin->fn == NULL && JIT) {
    return 1;
  }
  if (JIT) {
    /* JIT */
    ret = plugin->fn(mem, mem_len);
  } else {
    /* Interpreted */
    ret = ubpf_exec(plugin->vm, mem, mem_len);
  } 

  /* printf("0x%"PRIx64"\n", ret); */

  return ret;
}
