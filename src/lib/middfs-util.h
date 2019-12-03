/* middfs-util.h -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_UTIL_H
#define __MIDDFS_UTIL_H

#include <string.h>
#include <stdint.h>

#define STRINGIFY(s) #s
#define TOSTRING(s) STRINGIFY(s)

#ifndef MAX
  #define MAX(i1, i2) ((i1) < (i2) ? (i2) : (i1))
#endif
#ifndef MIN
  #define MIN(i1, i2) ((i1) < (i2) ? (i1) : (i2))
#endif

#define ARRLEN(arr) (sizeof(arr) / sizeof(arr[0]))

size_t sizerem(size_t nbytes, size_t used);
size_t smin(size_t s1, size_t s2);
size_t smax(size_t s1, size_t s2);

int inet_connect(const char *IP_addr, int port);
void *memdup(const void *ptr, size_t size);

#endif
