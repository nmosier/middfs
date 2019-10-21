/* middfs-buf.h -- buffer functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_BUF_H
#define __MIDDFS_BUF_H

#include <stdio.h>

struct buffer {
  void *begin;
  void *ptr;
  void *end;
};

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
