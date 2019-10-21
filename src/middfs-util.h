/* middfs-util.h -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_UTIL_H
#define __MIDDFS_UTIL_H

#include <string.h>
#include <stdint.h>

struct buffer {
  void *begin;
  void *ptr;
  void *end;
};

size_t sizerem(size_t nbytes, size_t used);
size_t smin(size_t s1, size_t s2);
size_t smax(size_t s1, size_t s2);

#define MAX(i1, i2) ((i1) < (i2) ? (i2) : (i1))
#define MIN(i1, i2) ((i1) < (i2) ? (i1) : (i2))

size_t buffer_size(const struct buffer *buf);
size_t buffer_used(const struct buffer *buf);
size_t buffer_rem(const struct buffer *buf);
int buffer_resize(struct buffer *buf, size_t newsize);
void buffer_empty(struct buffer *buf);
void buffer_shift(struct buffer *buf, size_t shift);
void buffer_init(struct buffer *buf);
void buffer_delete(struct buffer *buf);
int buffer_increase(struct buffer *buf);
int buffer_isempty(const struct buffer *buf);
void buffer_advance(struct buffer *buf, size_t nbytes);



#endif
