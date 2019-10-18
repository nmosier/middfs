/* middfs-serial.c
 * Nicholas Mosier & Tommaso Monaco
 * 2019
 */

#include <string.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdlib.h>
#include <stddef.h>

#include "middfs-util.h"
#include "middfs-serial.h"
#include "middfs-rsrc.h"
#include "middfs-pkt.h"

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

size_t serialize_strp(const char **str, void *buf, size_t nbytes) {
  return serialize_str(*str, buf, nbytes);
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
  
  return len + 1;
}

size_t serialize_uint32(uint32_t uint, void *buf,
			size_t nbytes) {
  /* TODO: write serialize_uint deserialize_uint */
  
  if (sizeof(uint32_t) <= nbytes) {
    *((uint32_t *) buf) = htonl(uint);  
  }
  
  return sizeof(uint32_t);
}

size_t deserialize_uint32(const void *buf, size_t nbytes,
			  uint32_t uint, int *errp) {
  /* TODO: write serialize_uint deserialize_uint */
  
  /* bail on previous error */
  if (*errp) {
    return 0;
  }

  if (sizeof(uint32_t) <= nbytes) {
    ntohl(uint);
  }

  return sizeof(uint32_t);
}

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes) { 
#if 0
  size_t used = 0;

  used += serialize_str(rsrc->mr_owner, buf + used,
			 sizerem(nbytes, used));
  used += serialize_str(rsrc->mr_path, buf + used,
			 sizerem(nbytes, used));
  return used;
#else
  return serialize((const void *) rsrc, buf, nbytes, 2,
		   offsetof(struct rsrc, mr_owner), serialize_strp,
		   offsetof(struct rsrc, mr_path), serialize_strp
		   );
#endif
}


size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp) {
  size_t used = 0;

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


typedef size_t (*serialize_f)(const void *ptr, void *buf,
			      size_t nbytes);

/* variable list:  offset, memblen */
size_t serialize(const void *ptr, void *buf, size_t nbytes,
		 int nmemb, ...) {
  va_list ap;
  size_t used = 0;
  va_start(ap, nmemb);

  for (int memb = 0; memb < nmemb; ++memb) {
    size_t memblen = va_arg(ap, size_t);
    const void *membptr = (const void *)
      ((const char *) ptr + memblen);
    serialize_f serialfn = va_arg(ap, serialize_f);
    used += serialfn(membptr, buf + used, sizerem(nbytes, used));
  }

  va_end(ap);
  return used;
}

/* TODO -- serialization function for uint64_t */
size_t serialize_request(const struct middfs_request *req, void *buf,
			size_t nbytes) {
  size_t used = 0;

  /* TODO: write serialize_uint deserialize_uint */
  used += serialize_uint32((uint32_t) req->mreq_type, buf + used,
			   sizerem(nbytes, used));
  used += serialize_str(req->mreq_requester, buf + used,
			sizerem(nbytes, used));
  used += serialize_rsrc(req->rsrc, buf + used,
			   sizerem(nbytes, used));
  
  return used;
}

size_t deserialize_request(const void *buf, size_t nbytes,
			   struct middfs_request *req, int *errp) {

  size_t used = 0;

  /* bail on previous error */
  if (*errp) {
    return 0;
  }

  /* TODO: write serialize_uint deserialize_uint */
  used += deserialize_uint32(buf + used, sizerem(nbytes, used),
			     *(uint32_t *) &req->mreq_type, errp);
  used += deserialize_str(buf + used, sizerem(nbytes, used),
			  &req->mreq_requester, errp);
  used += deserialize_rsrc(buf + used, sizerem(nbytes, used),
			   req->rsrc, errp);

  return *errp ? 0 : used;
}

size_t serialize_pkt(const struct middfs_packet *pkt, void *buf,
			size_t nbytes) {

  size_t used = 0;
  
  used += serialize_uint32(pkt->mpkt_magic, buf + used,
			   sizerem(nbytes, used));
  used += serialize_uint32((uint32_t) pkt->mpkt_type, buf + used,
			   sizerem(nbytes, used));
  
  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
    used += serialize_request(&pkt->mpkt_request, buf + used,
			      sizerem(nbytes, used));
  case MPKT_CONNECT:
  case MPKT_DISCONNECT:
  case MPKT_NONE:  
  default:
    abort();
  }
 
  return used;

}


size_t deserialize_pkt(const void *buf, size_t nbytes,
		       struct middfs_packet *pkt, int *errp) {
  size_t used = 0;

  /* bail on previous error */
  if (*errp) {
    return 0;
  }

  used += deserialize_uint32(buf + used, sizerem(nbytes, used),
			      pkt->mpkt_magic, errp);
  used += deserialize_uint32(buf + used, sizerem(nbytes, used),
			     *(uint32_t *) &pkt->mpkt_type, errp);
  
  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
    used += deserialize_request(buf + used, sizerem(nbytes, used),
				&pkt->mpkt_request, errp);
  case MPKT_CONNECT:
  case MPKT_DISCONNECT:
  case MPKT_NONE:  
  default:
    abort();
  }

  return *errp ? 0 : used;
}

