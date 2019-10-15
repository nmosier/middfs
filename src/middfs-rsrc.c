/* middfs-rsrc.c
 * Nicholas Mosier & Tommaso Monaco
 * 2019
 */

#include <string.h>

#include "middfs-util.h"
#include "middfs-rsrc.h"


size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes) {
  size_t used = 0;

  used += serialize_str(rsrc->mr_owner, buf + used,
			sizerem(nbytes, used));
  used += serialize_str(rsrc->mr_path, buf + used,
			sizerem(nbytes, used));

  return used;
}

size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp) {
  size_t used = 0;
  ssize_t rem = nbytes;

  /* bail on previous error */
  if (*errp) {
    return 0;
  }
  
  used += deserialize_str(buf + used, sizerem(nbytes, used),
			  &rsrc->mr_owner, errp);
  used += deserialize_str(buf + used, sizerem(nbytes, used),
			  &rsrc->mr_path, errp);
  
  return *errp ? 0 : used;
}
