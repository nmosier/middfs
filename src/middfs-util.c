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

