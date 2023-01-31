/**
 * \file plugin_memory.c
 *
 * \brief Support for memory management within each plugin -- Plugins receive a
 * contiguous memory space from the host, in which they allocate and deallocate
 * content
 *
 */
#include "core/or/or.h"
#include "lib/michelfralloc/michelfralloc.h"
#include "core/or/plugin_memory.h"

#include "string.h"
#include <stdint.h>
#include <stdio.h>

#include <unistd.h>



#define MAGIC_NUMBER 0xa110ca7ab1e


/*static uint8_t *addr_from_index(memory_pool_t *mp, uint64_t i) {*/
	/*return mp->mem_start + (i * mp->size_of_each_block);*/
/*}*/

/*static uint64_t index_from_addr(memory_pool_t *mp, uint8_t *p) {*/
	/*return ((uint64_t)(p - mp->mem_start)) / mp->size_of_each_block;*/
/*}*/

/**
 *
 * Search for big enough free space on heap; return NULL otherwise
 */
void *my_plugin_malloc(plugin_t *plugin, unsigned int size) {
  tor_assert(plugin);
	plugin_dynamic_memory_pool_t *mp = plugin->memory_pool;

  tor_assert(mp);

  void *ptr = michelfralloc((plugin_dynamic_memory_pool_t *) mp, size);
  return ptr;

  /*if (size > mp->size_of_each_block - 8) {*/
    /*log_debug(LD_PLUGIN, "Asking for %u bytes by slots up to %" PRIu64 "!\n", size, mp->size_of_each_block - 8);*/
    /*return NULL;*/
  /*}*/
  /*if (mp->num_initialized < mp->num_of_blocks) {*/
    /*uint64_t *ptr = (uint64_t *) addr_from_index(mp, mp->num_initialized);*/
    /*[> Very important for the mp->next computation <]*/
    /**ptr = mp->num_initialized + 1;*/
    /*mp->num_initialized++;*/
  /*}*/

  /*void *ret = NULL;*/
  /*if (mp->num_free_blocks > 0) {*/
    /*ret = (void *) mp->next;*/
    /*mp->num_free_blocks--;*/
    /*if (mp->num_free_blocks > 0) {*/
      /*mp->next = addr_from_index(mp, *((uint64_t *)mp->next));*/
    /*} else {*/
      /*mp->next = NULL;*/
    /*}*/
  /*}*/

  /*if (ret) {*/
    /**((uint64_t *)ret) = MAGIC_NUMBER;*/
    /*ret += 8;*/
  /*} else {*/
    /*printf("Out of memory!\n");*/
  /*}*/

	/*return ret;*/

}

void *my_plugin_malloc_dbg(plugin_t *plugin, unsigned int size, char *file, int line) {
    void *p = my_plugin_malloc(plugin, size);
    log_debug(LD_PLUGIN, "MY MALLOC %s:%d = %p\n", file, line, p);
    return p;
}

void my_plugin_free_in_core(plugin_t *plugin, void *ptr) {
  tor_assert(plugin);
	plugin_dynamic_memory_pool_t *mp = plugin->memory_pool;

  tor_assert(mp);

  michelfree((plugin_dynamic_memory_pool_t *) mp, ptr);
	/*ptr = (uint64_t*) ptr - 8;*/
	/*if (*((uint64_t *) ptr) != MAGIC_NUMBER){*/
		/*log_debug(LD_PLUGIN, "MEMORY CORRUPTION: BAD METADATA: 0x%lx, ORIGINAL PTR: %p\n", *((uint64_t *) ptr), (uint64_t*)ptr + 8);*/
	/*}*/
	/*memory_pool_t *mp = p->memory_pool;*/
	/*if (mp->next != NULL) {*/
		/*(*(uint64_t *) ptr) = index_from_addr(mp, mp->next);*/
		/*if (!(mp->mem_start <= (uint8_t *) ptr && (uint8_t *) ptr < (mp->mem_start + (mp->num_of_blocks * mp->size_of_each_block)))) {*/
      /*log_debug(LD_PLUGIN, "MEMORY CORRUPTION: FREEING MEMORY (%p) NOT BELONGING TO THE PLUGIN\n", (uint64_t *)ptr + 8);*/
		/*}*/
		/*mp->next = (uint8_t *) ptr;*/
	/*}*/
  /*else {*/
		/*(*(uint64_t *) ptr) = mp->num_of_blocks;*/
    /*if (!(mp->mem_start <= (uint8_t *) ptr && (uint8_t *) ptr < (mp->mem_start + (mp->num_of_blocks * mp->size_of_each_block)))) {*/
      /*log_debug(LD_PLUGIN, "MEMORY CORRUPTION: FREEING MEMORY (%p) NOT BELONGING TO THE PLUGIN\n", (uint64_t *)ptr + 8);*/
    /*}*/
		/*mp->next = (uint8_t *) ptr;*/
	/*}*/
	/*mp->num_free_blocks++;*/
}


/**
 * Frees the allocated memory. If first checks if the pointer falls
 * between the allocated heap range. It also checks if the pointer
 * to be deleted is actually allocated. this is done by using the
 * magic number. Due to lack of time i haven't worked on fragmentation.
 */ 
void my_plugin_free(plugin_t *plugin, void *ptr) {
  tor_assert(plugin);
	my_plugin_free_in_core(plugin, ptr);
}
void my_plugin_free_dbg(plugin_t *plugin, void *ptr, char *file, int line) {
  log_debug(LD_PLUGIN, "MY FREE %s:%d = %p\n", file, line, ptr);
  my_plugin_free(plugin, ptr);
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
void *my_plugin_realloc(plugin_t *plugin, void *ptr, unsigned int size) {
  tor_assert(plugin);
	plugin_dynamic_memory_pool_t *mp = plugin->memory_pool;

  tor_assert(mp);
	return michelfrealloc((plugin_dynamic_memory_pool_t *) mp, ptr, size);
  /*if (size > plugin->memory_pool->size_of_each_block - 8) {*/
		/*log_debug(LD_PLUGIN, "Asking for %u bytes by slots up to %lu!\n", size, plugin->memory_pool->size_of_each_block - 8);*/
		/*return NULL;*/
	/*}*/
}

/**
 * libc memcpy has some issues with plugins -- replacing it with this implem
 */

/*
 * sizeof(word) MUST BE A POWER OF TWO
 * SO THAT wmask BELOW IS ALL ONES
 */
typedef	int word;		/* "word" used for optimal copy speed */

#define	wsize	sizeof(word)
#define	wmask	(wsize - 1)

/*
 * Copy a block of memory, handling overlap.
 * This is the routine that actually implements
 * (the portable versions of) bcopy, memcpy, and memmove.
 */
void * my_plugin_memcpy(void *dst0, const void *src0, size_t length)
{
	char *dst = dst0;
	const char *src = src0;
	size_t t;

	if (length == 0 || dst == src)		/* nothing to do */
		goto done;

	/*
	 * Macros: loop-t-times; and loop-t-times, t>0
	 */
#define	TLOOP(s) if (t) TLOOP1(s)
#define	TLOOP1(s) do { s; } while (--t)

	if ((unsigned long)dst < (unsigned long)src) {
		/*
		 * Copy forward.
		 */
		t = (uintptr_t)src;	/* only need low bits */
		if ((t | (uintptr_t)dst) & wmask) {
			/*
			 * Try to align operands.  This cannot be done
			 * unless the low bits match.
			 */
			if ((t ^ (uintptr_t)dst) & wmask || length < wsize)
				t = length;
			else
				t = wsize - (t & wmask);
			length -= t;
			TLOOP1(*dst++ = *src++);
		}
		/*
		 * Copy whole words, then mop up any trailing bytes.
		 */
		t = length / wsize;
		TLOOP(*(word *)dst = *(word *)src; src += wsize; dst += wsize);
		t = length & wmask;
		TLOOP(*dst++ = *src++);
	} else {
		/*
		 * Copy backwards.  Otherwise essentially the same.
		 * Alignment works as before, except that it takes
		 * (t&wmask) bytes to align, not wsize-(t&wmask).
		 */
		src += length;
		dst += length;
		t = (uintptr_t)src;
		if ((t | (uintptr_t)dst) & wmask) {
			if ((t ^ (uintptr_t)dst) & wmask || length <= wsize)
				t = length;
			else
				t &= wmask;
			length -= t;
			TLOOP1(*--dst = *--src);
		}
		t = length / wsize;
		TLOOP(src -= wsize; dst -= wsize; *(word *)dst = *(word *)src);
		t = length & wmask;
		TLOOP(*--dst = *--src);
	}
done:
	return (dst0);
}

void * __attribute__((weak)) my_plugin_memset(void * dest, int c, size_t n)
{
    unsigned char *s = dest;
    size_t k;

    /* Fill head and tail with minimal branching. Each
     * conditional ensures that all the subsequently used
     * offsets are well-defined and in the dest region. */

    if (!n) return dest;
    s[0] = s[n-1] = c;
    if (n <= 2) return dest;
    s[1] = s[n-2] = c;
    s[2] = s[n-3] = c;
    if (n <= 6) return dest;
    s[3] = s[n-4] = c;
    if (n <= 8) return dest;

    /* Advance pointer to align it at a 4-byte boundary,
     * and truncate n to a multiple of 4. The previous code
     * already took care of any head/tail that get cut off
     * by the alignment. */

    k = -(uintptr_t)s & 3;
    s += k;
    n -= k;
    n &= -4;
    n /= 4;

    uint32_t *ws = (uint32_t *)s;
    uint32_t wc = c & 0xFF;
    wc |= ((wc << 8) | (wc << 16) | (wc << 24));

    /* Pure C fallback with no aliasing violations. */
    for (; n; n--, ws++) *ws = wc;

    return dest;
}


plugin_t *plugin_memory_init(size_t memory_size){
  plugin_t *plugin = tor_malloc_zero(sizeof(plugin_t));
  plugin->memory = tor_malloc_zero(memory_size);
  if (!plugin->memory){
    log_debug(LD_PLUGIN, "Could not instantiate the plugin's memory of %lu bytes", memory_size);
    return NULL;
  }
  plugin->memory_pool = (plugin_dynamic_memory_pool_t *) tor_calloc(1, sizeof(plugin_dynamic_memory_pool_t));
  if (!plugin->memory_pool) {
    tor_free(plugin->memory);
    log_debug(LD_PLUGIN, "Could not instantiate memory pool");
    return NULL;
  }
  plugin->memory_pool->memory_start = (uint8_t *) plugin->memory;
  plugin->memory_pool->memory_current_end = (uint8_t *) plugin->memory;
  plugin->memory_pool->memory_max_size = memory_size;
  plugin->memory_size = memory_size;
  /*plugin->entry_points = smartlist_new();*/
  return plugin;
}

void plugin_memory_free(plugin_t *plugin) {
  if (!plugin)
    return;
  if (plugin->memory_pool)
    tor_free(plugin->memory_pool);
  if (plugin->memory) {
    log_debug(LD_PLUGIN, "Freeing plugin's memory");
    tor_free(plugin->memory);
  }
}

