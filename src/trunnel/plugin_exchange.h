/* plugin_exchange.h -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#ifndef TRUNNEL_PLUGIN_EXCHANGE_H
#define TRUNNEL_PLUGIN_EXCHANGE_H

#include <stdint.h>
#include "trunnel.h"

#if !defined(TRUNNEL_OPAQUE) && !defined(TRUNNEL_OPAQUE_PLUGIN_FILE_PART)
struct plugin_file_part_st {
  uint16_t file_name_len;
  uint16_t data_len;
  uint8_t is_last_chunk;
  TRUNNEL_DYNARRAY_HEAD(, uint8_t) file_name;
  TRUNNEL_DYNARRAY_HEAD(, uint8_t) data;
  uint8_t trunnel_error_code_;
};
#endif
typedef struct plugin_file_part_st plugin_file_part_t;
/** Return a newly allocated plugin_file_part with all elements set to
 * zero.
 */
plugin_file_part_t *plugin_file_part_new(void);
/** Release all storage held by the plugin_file_part in 'victim'. (Do
 * nothing if 'victim' is NULL.)
 */
void plugin_file_part_free(plugin_file_part_t *victim);
/** Try to parse a plugin_file_part from the buffer in 'input', using
 * up to 'len_in' bytes from the input buffer. On success, return the
 * number of bytes consumed and set *output to the newly allocated
 * plugin_file_part_t. On failure, return -2 if the input appears
 * truncated, and -1 if the input is otherwise invalid.
 */
ssize_t plugin_file_part_parse(plugin_file_part_t **output, const uint8_t *input, const size_t len_in);
/** Return the number of bytes we expect to need to encode the
 * plugin_file_part in 'obj'. On failure, return a negative value.
 * Note that this value may be an overestimate, and can even be an
 * underestimate for certain unencodeable objects.
 */
ssize_t plugin_file_part_encoded_len(const plugin_file_part_t *obj);
/** Try to encode the plugin_file_part from 'input' into the buffer at
 * 'output', using up to 'avail' bytes of the output buffer. On
 * success, return the number of bytes used. On failure, return -2 if
 * the buffer was not long enough, and -1 if the input was invalid.
 */
ssize_t plugin_file_part_encode(uint8_t *output, size_t avail, const plugin_file_part_t *input);
/** Check whether the internal state of the plugin_file_part in 'obj'
 * is consistent. Return NULL if it is, and a short message if it is
 * not.
 */
const char *plugin_file_part_check(const plugin_file_part_t *obj);
/** Clear any errors that were set on the object 'obj' by its setter
 * functions. Return true iff errors were cleared.
 */
int plugin_file_part_clear_errors(plugin_file_part_t *obj);
/** Return the value of the file_name_len field of the
 * plugin_file_part_t in 'inp'
 */
uint16_t plugin_file_part_get_file_name_len(const plugin_file_part_t *inp);
/** Set the value of the file_name_len field of the plugin_file_part_t
 * in 'inp' to 'val'. Return 0 on success; return -1 and set the error
 * code on 'inp' on failure.
 */
int plugin_file_part_set_file_name_len(plugin_file_part_t *inp, uint16_t val);
/** Return the value of the data_len field of the plugin_file_part_t
 * in 'inp'
 */
uint16_t plugin_file_part_get_data_len(const plugin_file_part_t *inp);
/** Set the value of the data_len field of the plugin_file_part_t in
 * 'inp' to 'val'. Return 0 on success; return -1 and set the error
 * code on 'inp' on failure.
 */
int plugin_file_part_set_data_len(plugin_file_part_t *inp, uint16_t val);
/** Return the value of the is_last_chunk field of the
 * plugin_file_part_t in 'inp'
 */
uint8_t plugin_file_part_get_is_last_chunk(const plugin_file_part_t *inp);
/** Set the value of the is_last_chunk field of the plugin_file_part_t
 * in 'inp' to 'val'. Return 0 on success; return -1 and set the error
 * code on 'inp' on failure.
 */
int plugin_file_part_set_is_last_chunk(plugin_file_part_t *inp, uint8_t val);
/** Return the length of the dynamic array holding the file_name field
 * of the plugin_file_part_t in 'inp'.
 */
size_t plugin_file_part_getlen_file_name(const plugin_file_part_t *inp);
/** Return the element at position 'idx' of the dynamic array field
 * file_name of the plugin_file_part_t in 'inp'.
 */
uint8_t plugin_file_part_get_file_name(plugin_file_part_t *inp, size_t idx);
/** As plugin_file_part_get_file_name, but take and return a const
 * pointer
 */
uint8_t plugin_file_part_getconst_file_name(const plugin_file_part_t *inp, size_t idx);
/** Change the element at position 'idx' of the dynamic array field
 * file_name of the plugin_file_part_t in 'inp', so that it will hold
 * the value 'elt'.
 */
int plugin_file_part_set_file_name(plugin_file_part_t *inp, size_t idx, uint8_t elt);
/** Append a new element 'elt' to the dynamic array field file_name of
 * the plugin_file_part_t in 'inp'.
 */
int plugin_file_part_add_file_name(plugin_file_part_t *inp, uint8_t elt);
/** Return a pointer to the variable-length array field file_name of
 * 'inp'.
 */
uint8_t * plugin_file_part_getarray_file_name(plugin_file_part_t *inp);
/** As plugin_file_part_get_file_name, but take and return a const
 * pointer
 */
const uint8_t  * plugin_file_part_getconstarray_file_name(const plugin_file_part_t *inp);
/** Change the length of the variable-length array field file_name of
 * 'inp' to 'newlen'.Fill extra elements with 0. Return 0 on success;
 * return -1 and set the error code on 'inp' on failure.
 */
int plugin_file_part_setlen_file_name(plugin_file_part_t *inp, size_t newlen);
/** Return the length of the dynamic array holding the data field of
 * the plugin_file_part_t in 'inp'.
 */
size_t plugin_file_part_getlen_data(const plugin_file_part_t *inp);
/** Return the element at position 'idx' of the dynamic array field
 * data of the plugin_file_part_t in 'inp'.
 */
uint8_t plugin_file_part_get_data(plugin_file_part_t *inp, size_t idx);
/** As plugin_file_part_get_data, but take and return a const pointer
 */
uint8_t plugin_file_part_getconst_data(const plugin_file_part_t *inp, size_t idx);
/** Change the element at position 'idx' of the dynamic array field
 * data of the plugin_file_part_t in 'inp', so that it will hold the
 * value 'elt'.
 */
int plugin_file_part_set_data(plugin_file_part_t *inp, size_t idx, uint8_t elt);
/** Append a new element 'elt' to the dynamic array field data of the
 * plugin_file_part_t in 'inp'.
 */
int plugin_file_part_add_data(plugin_file_part_t *inp, uint8_t elt);
/** Return a pointer to the variable-length array field data of 'inp'.
 */
uint8_t * plugin_file_part_getarray_data(plugin_file_part_t *inp);
/** As plugin_file_part_get_data, but take and return a const pointer
 */
const uint8_t  * plugin_file_part_getconstarray_data(const plugin_file_part_t *inp);
/** Change the length of the variable-length array field data of 'inp'
 * to 'newlen'.Fill extra elements with 0. Return 0 on success; return
 * -1 and set the error code on 'inp' on failure.
 */
int plugin_file_part_setlen_data(plugin_file_part_t *inp, size_t newlen);


#endif
