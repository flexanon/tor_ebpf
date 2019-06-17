#ifndef PLUGIN_MEMORY_H
#define PLUGIN_MEMORY_H
#include "core/or/or.h"

void *my_plugin_malloc(tor_cnx_t *cnx, unsigned int size);
void *my_plugin_malloc_dbg(tor_cnx_t *cnx, unsigned int size, char *file, int line);
void my_plugin_free(tor_cnx_t *cnx, void *ptr);
void my_plugin_free_dbg(tor_cnx_t *cnx, void *ptr, char *file, int line);
void *my_plugin_realloc(tor_cnx_t *cnx, void *ptr, unsigned int size);
plugin_t *plugin_memory_init(void);

void my_plugin_free_in_core(plugin_t *p, void *ptr);

#endif // PLUGIN_MEMORY_H
