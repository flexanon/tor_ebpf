/*
 * Copyright 2015 Big Switch Networks, Inc
 * Copyright 2018 Quentin De Coninck
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef UBPF_H
#define UBPF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "core/or/or.h"

struct ubpf_vm;

struct ubpf_vm *ubpf_create(void);
void ubpf_destroy(struct ubpf_vm *vm);

/*
 * Register an external function
 *
 * The immediate field of a CALL instruction is an index into an array of
 * functions registered by the user. This API associates a function with
 * an index.
 *
 * 'name' should be a string with a lifetime longer than the VM.
 *
 * Returns 0 on success, -1 on error.
 */
int ubpf_register(struct ubpf_vm *vm, unsigned int idx, const char *name, void *fn);

/*
 * Load code into a VM
 *
 * This must be done before calling ubpf_exec or ubpf_compile and after
 * registering all functions.
 *
 * 'code' should point to eBPF bytecodes and 'code_len' should be the size in
 * bytes of that buffer.
 *
 * Returns 0 on success, -1 on error. In case of error a pointer to the error
 * message will be stored in 'errmsg' and should be freed by the caller.
 */
int ubpf_load(struct ubpf_vm *vm, const void *code, uint32_t code_len, char
    **errmsg, uint64_t memory_ptr, size_t memory_size, uint64_t ctx_id);

/*
 * Load code from an ELF file
 *
 * This must be done before calling ubpf_exec or ubpf_compile and after
 * registering all functions.
 *
 * 'elf' should point to a copy of an ELF file in memory and 'elf_len' should
 * be the size in bytes of that buffer.
 *
 * The ELF file must be 64-bit little-endian with a single text section
 * containing the eBPF bytecodes. This is compatible with the output of
 * Clang.
 *
 * Returns 0 on success, -1 on error. In case of error a pointer to the error
 * message will be stored in 'errmsg' and should be freed by the caller.
 */
int ubpf_load_elf(struct ubpf_vm *vm, const void *elf, size_t elf_len, char
    **errmsg, uint64_t memory_ptr, size_t memory_size, uint64_t ctx_id);

uint64_t ubpf_exec(struct ubpf_vm *vm, void *mem, size_t mem_len);

/*
 * Provide arg to R1, but ensure store and load access remains in the range
 * [mem, mem + mem_len[.
 */
uint64_t ubpf_exec_with_arg(struct ubpf_vm *vm, void *arg, void *mem, size_t mem_len);

ubpf_jit_fn ubpf_compile(struct ubpf_vm *vm, char **errmsg);

/*
 * Return the cause of the error if the VM crashed, or NULL otherwise
 */
const char *ubpf_get_error_msg(struct ubpf_vm *vm);

/* Now functions that will be actually used in the program */

int load_elf(void *code, size_t code_len, plugin_t *plugin, plugin_entry_point_t *entry_point);

MOCK_DECL(int, load_elf_file, (const char *code_filename, plugin_t *plugin, plugin_entry_point_t *entry_point));
void release_elf(plugin_entry_point_t *entry_point);
uint64_t exec_loaded_code(plugin_entry_point_t *plugin, void *mem, size_t mem_len);

#endif
