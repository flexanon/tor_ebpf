#include "core/or/or.h"
#include "ubpf/vm/plugin_memory.h"

#include <unistd.h>

#define MAGIC_NUMBER 0xa110ca7ab1e


static uint8_t *addr_from_index(memory_pool_t *mp, uint64_t i) {
	return mp->mem_start + (i * mp->size_of_each_block);
}

static uint64_t index_from_addr(memory_pool_t *mp, uint8_t *p) {
	return ((uint64_t)(p - mp->mem_start)) / mp->size_of_each_block;
}

/**
* Search for big enough free space on heap.
* Split the free space slot if it is too big, else space will be wasted.
* Return the pointer to this slot.
* If no adequately large free slot is available, extend the heap and return the pointer.
*/
void *my_plugin_malloc(tor_cnx_t *cnx, unsigned int size) {
	plugin_t *plugin = cnx->current_plugin;
  tor_assert(plugin);
	memory_pool_t *mp = plugin->memory_pool;
	if (size > mp->size_of_each_block - 8) {
		log_debug(LD_PLUGIN, "Asking for %u bytes by slots up to %lu!\n",
        size, mp->size_of_each_block - 8);
		return NULL;
	}
	if (mp->num_initialized < mp->num_of_blocks) {
		uint64_t *p = (uint64_t *) addr_from_index(mp, mp->num_initialized);
		/* Very important for the mp->next computation */
		*p = mp->num_initialized + 1;
		mp->num_initialized++;
	}

  //tor_assert_nonfatal(mp->num_free_blocks > 0);
	void *ret = NULL;
	if (mp->num_free_blocks > 0) {
		ret = (void *) mp->next;
		mp->num_free_blocks--;
    if (mp->num_free_blocks > 0) {
			mp->next = addr_from_index(mp, *((uint64_t *)mp->next));
		} else {
			mp->next = NULL;
		}
	} else {
		log_debug(LD_PLUGIN, "Out of memory!");
		mp->next = NULL;
    return NULL;
	}
	*((uint64_t *)ret) = MAGIC_NUMBER;
	return (uint64_t*) ret + 8;
}

void *my_plugin_malloc_dbg(tor_cnx_t *cnx, unsigned int size, char *file, int line) {
    void *p = my_plugin_malloc(cnx, size);
    log_debug(LD_PLUGIN, "MY MALLOC %s:%d = %p\n", file, line, p);
    return p;
}

void my_plugin_free_in_core(plugin_t *p, void *ptr) {
	ptr = (uint64_t*) ptr - 8;
	if (*((uint64_t *) ptr) != MAGIC_NUMBER){
		log_debug(LD_PLUGIN, "MEMORY CORRUPTION: BAD METADATA: 0x%lx, ORIGINAL PTR: %p\n", *((uint64_t *) ptr), (uint64_t*)ptr + 8);
	}
	memory_pool_t *mp = p->memory_pool;
	if (mp->next != NULL) {
		(*(uint64_t *) ptr) = index_from_addr(mp, mp->next);
		if (!(mp->mem_start <= (uint8_t *) ptr && (uint8_t *) ptr < (mp->mem_start + (mp->num_of_blocks * mp->size_of_each_block)))) {
      log_debug(LD_PLUGIN, "MEMORY CORRUPTION: FREEING MEMORY (%p) NOT BELONGING TO THE PLUGIN\n", (uint64_t *)ptr + 8);
		}
		mp->next = (uint8_t *) ptr;
	}
  else {
		(*(uint64_t *) ptr) = mp->num_of_blocks;
    if (!(mp->mem_start <= (uint8_t *) ptr && (uint8_t *) ptr < (mp->mem_start + (mp->num_of_blocks * mp->size_of_each_block)))) {
      log_debug(LD_PLUGIN, "MEMORY CORRUPTION: FREEING MEMORY (%p) NOT BELONGING TO THE PLUGIN\n", (uint64_t *)ptr + 8);
    }
		mp->next = (uint8_t *) ptr;
	}
	mp->num_free_blocks++;
}


/**
 * Frees the allocated memory. If first checks if the pointer falls
 * between the allocated heap range. It also checks if the pointer
 * to be deleted is actually allocated. this is done by using the
 * magic number. Due to lack of time i haven't worked on fragmentation.
 */ 
void my_plugin_free(tor_cnx_t *cnx, void *ptr) {
	plugin_t *p = cnx->current_plugin;
  tor_assert(p);
	my_plugin_free_in_core(p, ptr);
}
void my_plugin_free_dbg(tor_cnx_t *cnx, void *ptr, char *file, int line) {
  log_debug(LD_PLUGIN, "MY FREE %s:%d = %p\n", file, line, ptr);
  my_plugin_free(cnx, ptr);
}
/**
 * Reallocate the allocated memory to change its size. Three cases are possible.
 * 1) Asking for lower or equal size, or larger size without any block after.
 *    The block is left untouched, we simply increase its size.
 * 2) Asking for larger size, and another block is behind.
 *    We need to request another larger block, then copy the data and finally free it.
 * 3) Asking for larger size, without being able to have free space.
 *    Free the pointer and return NULL.
 * If an invalid pointer is provided, it returns NULL without changing anything.
 */
void *my_plugin_realloc(tor_cnx_t *cnx, void *ptr, unsigned int size) {
	plugin_t *p = cnx->current_plugin;
  tor_assert(p);
	// we cannot change the size of the block: if the new size is above the maximum, print an error,
	// otherwise, return the same pointer
	if (size > p->memory_pool->size_of_each_block - 8) {
		log_debug(LD_PLUGIN, "Asking for %u bytes by slots up to %lu!\n", size, p->memory_pool->size_of_each_block - 8);
		return NULL;
	}
	return ptr;
}

plugin_t *plugin_memory_init(void){
  plugin_t *plugin = tor_malloc_zero(sizeof(plugin_t));
  plugin->memory_pool = (memory_pool_t*) tor_malloc_zero(sizeof(memory_pool_t));
  plugin->memory_pool->mem_start = (uint8_t *) plugin->memory;
	plugin->memory_pool->size_of_each_block = 2100; /* TEST */
	plugin->memory_pool->num_of_blocks = PLUGIN_MEMORY / 2100;
	plugin->memory_pool->num_initialized = 0;
	plugin->memory_pool->num_free_blocks = plugin->memory_pool->num_of_blocks;
	plugin->memory_pool->next = plugin->memory_pool->mem_start;
  
  return plugin;
}
