#ifndef PLUGIN_MEMORY_H
#define PLUGIN_MEMORY_H
#include "core/or/or.h"
#include <stddef.h>

void *my_plugin_malloc(plugin_t *plugin, unsigned int size);
void *my_plugin_malloc_dbg(plugin_t *plugin, unsigned int size, char *file, int line);
void my_plugin_free(plugin_t *plugin, void *ptr);
void my_plugin_free_dbg(plugin_t *plugin, void *ptr, char *file, int line);
void *my_plugin_realloc(plugin_t *plugin, void *ptr, unsigned int size);

/********************************************************************
 ** memcpy, memset are an implementation of Danield Vik
 **
 ** Copyright (C) 2005 Daniel Vik
 **
 ** Description: Implementation of the standard library function memcpy.
 **             This implementation of memcpy() is ANSI-C89 compatible.
 **
 *******************************************************************/

void *my_plugin_memcpy(void *dest, const void *src, size_t count);
void *my_plugin_memset(void *dest, int c, size_t count);

plugin_t *plugin_memory_init(size_t memory_size);
void plugin_memory_free(plugin_t *plugin);


void my_plugin_free_in_core(plugin_t *p, void *ptr);

#endif // PLUGIN_MEMORY_H
