/* plug_cell.c -- generated by Trunnel v1.5.3.
 * https://gitweb.torproject.org/trunnel.git
 * You probably shouldn't edit this file.
 */
#include <stdlib.h>
#include "trunnel-impl.h"

#include "plug_cell.h"

#define TRUNNEL_SET_ERROR_CODE(obj) \
  do {                              \
    (obj)->trunnel_error_code_ = 1; \
  } while (0)

#if defined(__COVERITY__) || defined(__clang_analyzer__)
/* If we're running a static analysis tool, we don't want it to complain
 * that some of our remaining-bytes checks are dead-code. */
int plugcell_deadcode_dummy__ = 0;
#define OR_DEADCODE_DUMMY || plugcell_deadcode_dummy__
#else
#define OR_DEADCODE_DUMMY
#endif

#define CHECK_REMAINING(nbytes, label)                           \
  do {                                                           \
    if (remaining < (nbytes) OR_DEADCODE_DUMMY) {                \
      goto label;                                                \
    }                                                            \
  } while (0)

plugin_part_t *
plugin_part_new(void)
{
  plugin_part_t *val = trunnel_calloc(1, sizeof(plugin_part_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
plugin_part_clear(plugin_part_t *obj)
{
  (void) obj;
  TRUNNEL_DYNARRAY_WIPE(&obj->plugin_data_part);
  TRUNNEL_DYNARRAY_CLEAR(&obj->plugin_data_part);
}

void
plugin_part_free(plugin_part_t *obj)
{
  if (obj == NULL)
    return;
  plugin_part_clear(obj);
  trunnel_memwipe(obj, sizeof(plugin_part_t));
  trunnel_free_(obj);
}

uint64_t
plugin_part_get_total_len(const plugin_part_t *inp)
{
  return inp->total_len;
}
int
plugin_part_set_total_len(plugin_part_t *inp, uint64_t val)
{
  inp->total_len = val;
  return 0;
}
uint16_t
plugin_part_get_data_len(const plugin_part_t *inp)
{
  return inp->data_len;
}
int
plugin_part_set_data_len(plugin_part_t *inp, uint16_t val)
{
  inp->data_len = val;
  return 0;
}
uint64_t
plugin_part_get_offset(const plugin_part_t *inp)
{
  return inp->offset;
}
int
plugin_part_set_offset(plugin_part_t *inp, uint64_t val)
{
  inp->offset = val;
  return 0;
}
size_t
plugin_part_getlen_plugin_data_part(const plugin_part_t *inp)
{
  return TRUNNEL_DYNARRAY_LEN(&inp->plugin_data_part);
}

uint8_t
plugin_part_get_plugin_data_part(plugin_part_t *inp, size_t idx)
{
  return TRUNNEL_DYNARRAY_GET(&inp->plugin_data_part, idx);
}

uint8_t
plugin_part_getconst_plugin_data_part(const plugin_part_t *inp, size_t idx)
{
  return plugin_part_get_plugin_data_part((plugin_part_t*)inp, idx);
}
int
plugin_part_set_plugin_data_part(plugin_part_t *inp, size_t idx, uint8_t elt)
{
  TRUNNEL_DYNARRAY_SET(&inp->plugin_data_part, idx, elt);
  return 0;
}
int
plugin_part_add_plugin_data_part(plugin_part_t *inp, uint8_t elt)
{
#if SIZE_MAX >= UINT16_MAX
  if (inp->plugin_data_part.n_ == UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  TRUNNEL_DYNARRAY_ADD(uint8_t, &inp->plugin_data_part, elt, {});
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}

uint8_t *
plugin_part_getarray_plugin_data_part(plugin_part_t *inp)
{
  return inp->plugin_data_part.elts_;
}
const uint8_t  *
plugin_part_getconstarray_plugin_data_part(const plugin_part_t *inp)
{
  return (const uint8_t  *)plugin_part_getarray_plugin_data_part((plugin_part_t*)inp);
}
int
plugin_part_setlen_plugin_data_part(plugin_part_t *inp, size_t newlen)
{
  uint8_t *newptr;
#if UINT16_MAX < SIZE_MAX
  if (newlen > UINT16_MAX)
    goto trunnel_alloc_failed;
#endif
  newptr = trunnel_dynarray_setlen(&inp->plugin_data_part.allocated_,
                 &inp->plugin_data_part.n_, inp->plugin_data_part.elts_, newlen,
                 sizeof(inp->plugin_data_part.elts_[0]), (trunnel_free_fn_t) NULL,
                 &inp->trunnel_error_code_);
  if (newlen != 0 && newptr == NULL)
    goto trunnel_alloc_failed;
  inp->plugin_data_part.elts_ = newptr;
  return 0;
 trunnel_alloc_failed:
  TRUNNEL_SET_ERROR_CODE(inp);
  return -1;
}
const char *
plugin_part_check(const plugin_part_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (TRUNNEL_DYNARRAY_LEN(&obj->plugin_data_part) != obj->data_len)
    return "Length mismatch for plugin_data_part";
  return NULL;
}

ssize_t
plugin_part_encoded_len(const plugin_part_t *obj)
{
  ssize_t result = 0;

  if (NULL != plugin_part_check(obj))
     return -1;


  /* Length of u64 total_len */
  result += 8;

  /* Length of u16 data_len */
  result += 2;

  /* Length of u64 offset */
  result += 8;

  /* Length of u8 plugin_data_part[data_len] */
  result += TRUNNEL_DYNARRAY_LEN(&obj->plugin_data_part);
  return result;
}
int
plugin_part_clear_errors(plugin_part_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
plugin_part_encode(uint8_t *output, const size_t avail, const plugin_part_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = plugin_part_encoded_len(obj);
#endif

  if (NULL != (msg = plugin_part_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u64 total_len */
  trunnel_assert(written <= avail);
  if (avail - written < 8)
    goto truncated;
  trunnel_set_uint64(ptr, trunnel_htonll(obj->total_len));
  written += 8; ptr += 8;

  /* Encode u16 data_len */
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->data_len));
  written += 2; ptr += 2;

  /* Encode u64 offset */
  trunnel_assert(written <= avail);
  if (avail - written < 8)
    goto truncated;
  trunnel_set_uint64(ptr, trunnel_htonll(obj->offset));
  written += 8; ptr += 8;

  /* Encode u8 plugin_data_part[data_len] */
  {
    size_t elt_len = TRUNNEL_DYNARRAY_LEN(&obj->plugin_data_part);
    trunnel_assert(obj->data_len == elt_len);
    trunnel_assert(written <= avail);
    if (avail - written < elt_len)
      goto truncated;
    if (elt_len)
      memcpy(ptr, obj->plugin_data_part.elts_, elt_len);
    written += elt_len; ptr += elt_len;
  }


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As plugin_part_parse(), but do not allocate the output object.
 */
static ssize_t
plugin_part_parse_into(plugin_part_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u64 total_len */
  CHECK_REMAINING(8, truncated);
  obj->total_len = trunnel_ntohll(trunnel_get_uint64(ptr));
  remaining -= 8; ptr += 8;

  /* Parse u16 data_len */
  CHECK_REMAINING(2, truncated);
  obj->data_len = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;

  /* Parse u64 offset */
  CHECK_REMAINING(8, truncated);
  obj->offset = trunnel_ntohll(trunnel_get_uint64(ptr));
  remaining -= 8; ptr += 8;

  /* Parse u8 plugin_data_part[data_len] */
  CHECK_REMAINING(obj->data_len, truncated);
  TRUNNEL_DYNARRAY_EXPAND(uint8_t, &obj->plugin_data_part, obj->data_len, {});
  obj->plugin_data_part.n_ = obj->data_len;
  if (obj->data_len)
    memcpy(obj->plugin_data_part.elts_, ptr, obj->data_len);
  ptr += obj->data_len; remaining -= obj->data_len;
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 trunnel_alloc_failed:
  return -1;
}

ssize_t
plugin_part_parse(plugin_part_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = plugin_part_new();
  if (NULL == *output)
    return -1;
  result = plugin_part_parse_into(*output, input, len_in);
  if (result < 0) {
    plugin_part_free(*output);
    *output = NULL;
  }
  return result;
}
plug_cell_t *
plug_cell_new(void)
{
  plug_cell_t *val = trunnel_calloc(1, sizeof(plug_cell_t));
  if (NULL == val)
    return NULL;
  return val;
}

/** Release all storage held inside 'obj', but do not free 'obj'.
 */
static void
plug_cell_clear(plug_cell_t *obj)
{
  (void) obj;
  plugin_part_free(obj->data_ppart);
  obj->data_ppart = NULL;
}

void
plug_cell_free(plug_cell_t *obj)
{
  if (obj == NULL)
    return;
  plug_cell_clear(obj);
  trunnel_memwipe(obj, sizeof(plug_cell_t));
  trunnel_free_(obj);
}

uint8_t
plug_cell_get_version(const plug_cell_t *inp)
{
  return inp->version;
}
int
plug_cell_set_version(plug_cell_t *inp, uint8_t val)
{
  if (! ((val == 0 || val == 1))) {
     TRUNNEL_SET_ERROR_CODE(inp);
     return -1;
  }
  inp->version = val;
  return 0;
}
uint64_t
plug_cell_get_uid(const plug_cell_t *inp)
{
  return inp->uid;
}
int
plug_cell_set_uid(plug_cell_t *inp, uint64_t val)
{
  inp->uid = val;
  return 0;
}
uint16_t
plug_cell_get_length(const plug_cell_t *inp)
{
  return inp->length;
}
int
plug_cell_set_length(plug_cell_t *inp, uint16_t val)
{
  inp->length = val;
  return 0;
}
struct plugin_part_st *
plug_cell_get_data_ppart(plug_cell_t *inp)
{
  return inp->data_ppart;
}
const struct plugin_part_st *
plug_cell_getconst_data_ppart(const plug_cell_t *inp)
{
  return plug_cell_get_data_ppart((plug_cell_t*) inp);
}
int
plug_cell_set_data_ppart(plug_cell_t *inp, struct plugin_part_st *val)
{
  if (inp->data_ppart && inp->data_ppart != val)
    plugin_part_free(inp->data_ppart);
  return plug_cell_set0_data_ppart(inp, val);
}
int
plug_cell_set0_data_ppart(plug_cell_t *inp, struct plugin_part_st *val)
{
  inp->data_ppart = val;
  return 0;
}
const char *
plug_cell_check(const plug_cell_t *obj)
{
  if (obj == NULL)
    return "Object was NULL";
  if (obj->trunnel_error_code_)
    return "A set function failed on this object";
  if (! (obj->version == 0 || obj->version == 1))
    return "Integer out of bounds";
  switch (obj->version) {

    case 0:
      break;

    case 1:
      {
        const char *msg;
        if (NULL != (msg = plugin_part_check(obj->data_ppart)))
          return msg;
      }
      break;

    default:
        return "Bad tag for union";
      break;
  }
  return NULL;
}

ssize_t
plug_cell_encoded_len(const plug_cell_t *obj)
{
  ssize_t result = 0;

  if (NULL != plug_cell_check(obj))
     return -1;


  /* Length of u8 version IN [0, 1] */
  result += 1;

  /* Length of u64 uid */
  result += 8;

  /* Length of u16 length */
  result += 2;
  switch (obj->version) {

    case 0:
      break;

    case 1:

      /* Length of struct plugin_part data_ppart */
      result += plugin_part_encoded_len(obj->data_ppart);
      break;

    default:
      trunnel_assert(0);
      break;
  }
  return result;
}
int
plug_cell_clear_errors(plug_cell_t *obj)
{
  int r = obj->trunnel_error_code_;
  obj->trunnel_error_code_ = 0;
  return r;
}
ssize_t
plug_cell_encode(uint8_t *output, const size_t avail, const plug_cell_t *obj)
{
  ssize_t result = 0;
  size_t written = 0;
  uint8_t *ptr = output;
  const char *msg;
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  const ssize_t encoded_len = plug_cell_encoded_len(obj);
#endif

  uint8_t *backptr_length = NULL;

  if (NULL != (msg = plug_cell_check(obj)))
    goto check_failed;

#ifdef TRUNNEL_CHECK_ENCODED_LEN
  trunnel_assert(encoded_len >= 0);
#endif

  /* Encode u8 version IN [0, 1] */
  trunnel_assert(written <= avail);
  if (avail - written < 1)
    goto truncated;
  trunnel_set_uint8(ptr, (obj->version));
  written += 1; ptr += 1;

  /* Encode u64 uid */
  trunnel_assert(written <= avail);
  if (avail - written < 8)
    goto truncated;
  trunnel_set_uint64(ptr, trunnel_htonll(obj->uid));
  written += 8; ptr += 8;

  /* Encode u16 length */
  backptr_length = ptr;
  trunnel_assert(written <= avail);
  if (avail - written < 2)
    goto truncated;
  trunnel_set_uint16(ptr, trunnel_htons(obj->length));
  written += 2; ptr += 2;
  {
    size_t written_before_union = written;

    /* Encode union data[version] */
    trunnel_assert(written <= avail);
    switch (obj->version) {

      case 0:
        break;

      case 1:

        /* Encode struct plugin_part data_ppart */
        trunnel_assert(written <= avail);
        result = plugin_part_encode(ptr, avail - written, obj->data_ppart);
        if (result < 0)
          goto fail; /* XXXXXXX !*/
        written += result; ptr += result;
        break;

      default:
        trunnel_assert(0);
        break;
    }
    /* Write the length field back to length */
    trunnel_assert(written >= written_before_union);
#if UINT16_MAX < SIZE_MAX
    if (written - written_before_union > UINT16_MAX)
      goto check_failed;
#endif
    trunnel_set_uint16(backptr_length, trunnel_htons(written - written_before_union));
  }


  trunnel_assert(ptr == output + written);
#ifdef TRUNNEL_CHECK_ENCODED_LEN
  {
    trunnel_assert(encoded_len >= 0);
    trunnel_assert((size_t)encoded_len == written);
  }

#endif

  return written;

 truncated:
  result = -2;
  goto fail;
 check_failed:
  (void)msg;
  result = -1;
  goto fail;
 fail:
  trunnel_assert(result < 0);
  return result;
}

/** As plug_cell_parse(), but do not allocate the output object.
 */
static ssize_t
plug_cell_parse_into(plug_cell_t *obj, const uint8_t *input, const size_t len_in)
{
  const uint8_t *ptr = input;
  size_t remaining = len_in;
  ssize_t result = 0;
  (void)result;

  /* Parse u8 version IN [0, 1] */
  CHECK_REMAINING(1, truncated);
  obj->version = (trunnel_get_uint8(ptr));
  remaining -= 1; ptr += 1;
  if (! (obj->version == 0 || obj->version == 1))
    goto fail;

  /* Parse u64 uid */
  CHECK_REMAINING(8, truncated);
  obj->uid = trunnel_ntohll(trunnel_get_uint64(ptr));
  remaining -= 8; ptr += 8;

  /* Parse u16 length */
  CHECK_REMAINING(2, truncated);
  obj->length = trunnel_ntohs(trunnel_get_uint16(ptr));
  remaining -= 2; ptr += 2;
  {
    size_t remaining_after;
    CHECK_REMAINING(obj->length, truncated);
    remaining_after = remaining - obj->length;
    remaining = obj->length;

    /* Parse union data[version] */
    switch (obj->version) {

      case 0:
        /* Skip to end of union */
        ptr += remaining; remaining = 0;
        break;

      case 1:

        /* Parse struct plugin_part data_ppart */
        result = plugin_part_parse(&obj->data_ppart, ptr, remaining);
        if (result < 0)
          goto fail;
        trunnel_assert((size_t)result <= remaining);
        remaining -= result; ptr += result;
        break;

      default:
        goto fail;
        break;
    }
    if (remaining != 0)
      goto fail;
    remaining = remaining_after;
  }
  trunnel_assert(ptr + remaining == input + len_in);
  return len_in - remaining;

 truncated:
  return -2;
 fail:
  result = -1;
  return result;
}

ssize_t
plug_cell_parse(plug_cell_t **output, const uint8_t *input, const size_t len_in)
{
  ssize_t result;
  *output = plug_cell_new();
  if (NULL == *output)
    return -1;
  result = plug_cell_parse_into(*output, input, len_in);
  if (result < 0) {
    plug_cell_free(*output);
    *output = NULL;
  }
  return result;
}
