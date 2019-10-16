/* middfs-util.c -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <string.h>
#include <errno.h>

/* sizerem() -- get remaining size given total size and used size.
 * RETV: if _nbytes_ >= _used_, returns _nbytes_ - _used_;
 *       otherwise returns 0.
 */
size_t sizerem(size_t nbytes, size_t used) {
  return nbytes >= used ? nbytes - used : 0;
}
