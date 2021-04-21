#ifndef PLUGIN_MEMORY_H
#define PLUGIN_MEMORY_H
#include "core/or/or.h"

void *my_plugin_malloc(plugin_t *plugin, unsigned int size);
void *my_plugin_malloc_dbg(plugin_t *plugin, unsigned int size, char *file, int line);
void my_plugin_free(plugin_t *plugin, void *ptr);
void my_plugin_free_dbg(plugin_t *plugin, void *ptr, char *file, int line);
void *my_plugin_realloc(plugin_t *plugin, void *ptr, unsigned int size);
plugin_t *plugin_memory_init(size_t memory_size);

void my_plugin_free_in_core(plugin_t *p, void *ptr);

#endif // PLUGIN_MEMORY_H
