/* middfs-buf.c -- buffer functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

#include "middfs-util.h"
#include "middfs-serial.h"
#include "middfs-buf.h"

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

/* buffer_read_once() -- read once from fd into buffer,
 *                       as many bytes as possible
 * ARGS:
 *  - fd: file descriptor to read(2) from
 *  - buf: pointer to buffer struct
 * RETV: see read(2)
 */
ssize_t buffer_read(int fd, struct buffer *buf) {
  /* make sure there is space in the buffer */
  if (buffer_increase(buf) < 0) {
    return -1;
  }

  size_t rem = buffer_rem(buf);
  ssize_t bytes_read;

  if ((bytes_read = read(fd, buf->ptr, rem)) >= 0) {
    /* advance buffer pointer if read was successful */
    buffer_advance(buf, bytes_read);
  }

  return bytes_read;
}

/* buffer_write() -- write once from buffer to fd,
 *                   as many bytes as possible 
 * ARGS:
 *  - fd: file descriptor to write(2) to
 *  - buf: pointer to buffer struct
 */
ssize_t buffer_write(int fd, struct buffer *buf) {
  size_t used = buffer_used(buf);

  ssize_t bytes_written;

  if ((bytes_written = write(fd, buf->begin, used)) >= 0) {
    buffer_shift(buf, bytes_written);
  }

  return bytes_written;
}

/* buffer_copy() -- copy bytes into buffer, appending
 * ARGS:
 *  - buf: buffer to copy bytes into
 *  - in: input data ptr
 *  - nbytes: number of bytes to copy
 * RETV: -1 on error; 0 on success.
 */
ssize_t buffer_copy(struct buffer *buf, void *in, size_t nbytes) {
  size_t rem = buffer_rem(buf);

  if (rem < nbytes) {
    if (buffer_resize(buf, buffer_size(buf) + nbytes) < 0) {
      return -1;
    }
  }

  /* copy data & update pointer */
  memcpy(buf->ptr, in, nbytes);
  buffer_advance(buf, nbytes);

  return 0;
}

/* buffer_serialize() -- serialize datatype into buffer 
 * ARGS:
 *  - in: input data to be serialized
 *  - serialf: function to serialize data 
 *  - buf: buffer struct to operate on 
 * RETV: the number of bytes written, or -1 on error.
 */
ssize_t buffer_serialize(const void *in, serialize_f serialf, struct buffer *buf) {
  size_t used;
  size_t rem = buffer_rem(buf);
  
  /* compute number of bytes required */
  /* NOTE: This while-loop is intended to only run ONCE or TWICE.
   */
  while ((used = serialf(in, buf->ptr, rem)) > rem) {
    if (buffer_resize(buf, buffer_used(buf) + used) < 0) {
      return -1; /* buffer_resize() error */
    }
    rem = used;
  }

  /* advance pointer in buffer past written data */
  buffer_advance(buf, used);
  
  return used;
}
