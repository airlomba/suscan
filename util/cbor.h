/*
 * Copyright (c) 2017-2019 Josef 'Jeff' Sipek <jeffpc@josefsipek.net>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _UTIL_CBOR_H
#define _UTIL_CBOR_H

#include <errno.h>
#include <sigutils/types.h>

#define CBOR_UNKNOWN_NELEM  (~0ul)

/*
 * On failure, the buffer may contain partially encoded data items.  On
 * success, a fully encoded data item is appended to the buffer.
 */
int cbor_pack_uint(grow_buf_t *buffer, uint64_t v);
int cbor_pack_nint(grow_buf_t *buffer, uint64_t v);
int cbor_pack_int(grow_buf_t *buffer, int64_t v);
int cbor_pack_blob(grow_buf_t *buffer, const void *data,
        size_t size);
int cbor_pack_cstr_len(grow_buf_t *buffer, const char *str, size_t len);
int cbor_pack_str(grow_buf_t *buffer, const char *str);
int cbor_pack_bool(grow_buf_t *buffer, SUBOOL b);
int cbor_pack_null(grow_buf_t *buffer);
int cbor_pack_break(grow_buf_t *buffer);
int cbor_pack_array_start(grow_buf_t *buffer, size_t nelem);
int cbor_pack_array_end(grow_buf_t *buffer, size_t nelem);
int cbor_pack_map_start(grow_buf_t *buffer, size_t npairs);
int cbor_pack_map_end(grow_buf_t *buffer, size_t npairs);

SUINLINE
int cbor_pack_cstr(grow_buf_t *buffer, const char *str)
{
  return cbor_pack_cstr_len(buffer, str, strlen(str));
}


/*
 * On failure, the buffer state is unchanged.  On success, the buffer is
 * updated to point to the first byte of the next data item.
 */
int cbor_unpack_uint(grow_buf_t *buffer, uint64_t *v);
int cbor_unpack_nint(grow_buf_t *buffer, uint64_t *v);
int cbor_unpack_int(grow_buf_t *buffer, int64_t *v);
int cbor_unpack_blob(grow_buf_t *buffer, void **data, size_t *size);
int cbor_unpack_cstr_len(grow_buf_t *buffer, char **str,
        size_t *len);
int cbor_unpack_str(grow_buf_t *buffer, char **str);
int cbor_unpack_bool(grow_buf_t *buffer, SUBOOL *b);
int cbor_unpack_null(grow_buf_t *buffer);
int cbor_unpack_break(grow_buf_t *buffer);
int cbor_unpack_map_start(grow_buf_t *buffer, uint64_t *npairs,
                                 SUBOOL *end_required);
int cbor_unpack_map_end(grow_buf_t *buffer, SUBOOL end_required);
int cbor_unpack_array_start(grow_buf_t *buffer, uint64_t *nelem,
                                   SUBOOL *end_required);
int cbor_unpack_array_end(grow_buf_t *buffer, SUBOOL end_required);

#endif /* _UTIL_CBOR_H */
