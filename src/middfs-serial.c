/* middfs-serial.c
 * Nicholas Mosier & Tommaso Monaco
 * 2019
 */

#include <string.h>

#include "middfs-utils.h"
#include "middfs-serial.h"

/* SERIALIZATION FUNCTIONS 
 * serialize_*:
 *   These functions shall be of the format
 *      serialize_*(const serialize_t val, void *buf, size_t nbytes)
 *   where `serialize_t' is the type of the record being serialized.
 *   Their behavior shall be as follows:
 *     1.If the serialization of _val_ requires more than _nbytes_
 *       to be written to _buf_, serialize_*() shall return the 
 *       number of bytes required. (That way the caller can tell
 *       if _val_ was successfully serialized to _buf_ if
 *       the return value > _nbytes.)
 *     2.Otherwise, serialize_*() returns the number of bytes written.
 *       (Note that 1. and 2. describe the same return value.)
 * 
 * deserialize_*:
 *   These functions shall be of the format
 *     deserialize_*(const void *buf, size_t nbytes, serialize_t val)
 *   Their behavior shall be as follows:
 *     1.If the deserialization of _buf_ into _val_ requires more than
 *       the _nbytes_ currently in the buffer, deserialize_*() shall
 *       return _nbytes_ + 1 indicating more bytes are required. 
 *       Subsequent calls to deserialize_*() will then reattempt to 
 *       deserialize _buf_ into _val_ from the beginning.
 *     2.If the deserialization of _buf_ into _val_ succeeds,
 *       deserialize_*() shall return the number of bytes required for
 *       the deserialization.
 *       (Note that 1. and 2. do NOT describe the same values).
 */

size_t serialize_str(const char *str, void *buf, size_t nbytes) {
  size_t len;
  
  len = strnlen(str, nbytes);
  
  /* copy string, if there's room  */
  if (len < nbytes) {
    strcpy(buf, str);
  }
  
  return len + 1;
}


size_t deserialize_str(const void *buf, size_t nbytes,
		       char **strp, int *errp) {
  size_t len;

  /* bail on previous error */
  if (*errp) {
    return 0;
  }
  
  len = strnlen((const char *) buf, nbytes);

  /* allocate string */
  if (len < nbytes) {
    char *str = strdup((const char *) buf);
    if (str == NULL) {
      *errp = errno;
      return 0;
    }
    *strp = str;
  } else {
    len = nbytes + 1; /* indicate more bytes required */
  }
  
  return len;
}


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
