/* middfs-util.c -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>

#include "middfs-util.h"

/* sizerem() -- get remaining size given total size and used size.
 * RETV: if _nbytes_ >= _used_, returns _nbytes_ - _used_;
 *       otherwise returns 0.
 */
size_t sizerem(size_t nbytes, size_t used) {
  return nbytes >= used ? nbytes - used : 0;
}

size_t smin(size_t s1, size_t s2) {
  return (s1 < s2) ? s1 : s2;
}

size_t smax(size_t s1, size_t s2) {
  return (s1 < s2) ? s2 : s1;
}

/* Buffer Functions */

size_t buffer_size(const struct buffer *buf) {
  return (uint8_t *) buf->end - (uint8_t *) buf->begin;
}

size_t buffer_used(const struct buffer *buf) {
  return (uint8_t *) buf->ptr - (uint8_t *) buf->begin;
}

size_t buffer_rem(const struct buffer *buf) {
  return (uint8_t *) buf->end - (uint8_t *) buf->ptr;
}

int buffer_resize(struct buffer *buf, size_t newsize) {
  size_t used = buffer_used(buf);
  uint8_t *newptr;
  
  if ((newptr = realloc(buf->begin, newsize)) == NULL) {
    /* realloc error */
    return -errno;
  }

  buf->begin = newptr;
  buf->ptr = newptr + smin(used, newsize);
  buf->end = newptr + newsize;

  return 0;
}

/* buffer_empty() -- empty buffer */
void buffer_empty(struct buffer *buf) {
  buf->ptr = buf->begin;
}

/* buffer_shift() -- shift contents of buffer towards beginning
 * ARGS:
 *  - buf: buffer to operate on
 *  - shift: number of bytes to shift by */
void buffer_shift(struct buffer *buf, size_t shift) {
  size_t used = buffer_used(buf);

  if (shift >= used) {
    /* all data will be shifted out */
    buffer_empty(buf);
    return;
  } else {
    void *src = (uint8_t *) buf->begin + shift;
    size_t newused = used - shift; /* used bytes after shift */
    
    memmove(buf->begin, src, newused);
    
    buf->ptr = (uint8_t *) buf->begin + newused;
  }
}

void buffer_init(struct buffer *buf) {
  memset(buf, 0, sizeof(*buf));
}

/* buffer_increase() -- make sure there is free space in the buffer 
 *                    so data can be added */
int buffer_increase(struct buffer *buf) {
  size_t rem = buffer_rem(buf);

  if (rem == 0) {
    size_t size = buffer_size(buf);
    size_t newsize = smax(1, size * 2);
    
    return buffer_resize(buf, newsize);
    
  } else {
    return 0;
  }
}

void buffer_delete(struct buffer *buf) {
  free(buf->begin);
}

int buffer_isempty(const struct buffer *buf) {
  return buffer_used(buf) == 0;
}

void buffer_advance(struct buffer *buf, size_t nbytes) {
  size_t rem = buffer_rem(buf);

  /* check bounds */
  assert(rem >= nbytes);

  /* update buffer pointer */
  buf->ptr = (uint8_t *) buf->ptr + nbytes;
}
