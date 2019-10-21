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


/* serialize_union() -- serialize a union with base address
 *   _ptr_ into buffer _buf_.
 * ARGS:
 *  - ptr: base addresss of union
 *  - e: enum value to switch on
 *  - buf: pointer to serialization buffer
 *  - nbytes: the max number of bytes that should be written to
 *            _buf_
 *  - nmemb: the number of members in the union
 * VARARGS: There should be _nmemb_ triplets of varargs. Each triplet
 *          consists of the following:
 *  1. The offset of the member in the union.
 *  2. The enum value, passed in _e_, corresponding to this member.
 *  3. The serialization function for this member.
 */


/* variable list:  offset, memblen */
size_t serialize_struct(const void *ptr, void *buf, size_t nbytes,
			int nmemb, ...) {
  uint8_t *buf_ = (uint8_t *) buf;
  va_list ap;
  size_t used = 0;
  va_start(ap, nmemb);

  for (int memb = 0; memb < nmemb; ++memb) {
    size_t memblen = va_arg(ap, size_t);
    const void *membptr = (const void *)
      ((const char *) ptr + memblen);
    serialize_f serialfn = va_arg(ap, serialize_f);
    used += serialfn(membptr, buf_ + used, sizerem(nbytes, used));
  }

  va_end(ap);
  return used;
}



size_t deserialize_struct(const void *buf, size_t nbytes, void *ptr,
			  int *errp, int nmemb, ...) {
  const uint8_t *buf_ = (const uint8_t *) buf;
  uint8_t *ptr_ = (uint8_t *) ptr;
  va_list ap;
  size_t used = 0;

  /* check for previous error */
  if (*errp) {
    return 0;
  }
  
  va_start(ap, nmemb);

  for (int i = 0; i < nmemb; ++i) {
    size_t memboff = va_arg(ap, size_t);
    uint8_t *membptr = ptr_ + memboff;
    deserialize_f deserialfn = va_arg(ap, deserialize_f);
    used += deserialfn(buf_ + used, sizerem(nbytes, used),
		       membptr, errp);
  }

  va_end(ap);
  return used;
}


size_t serialize_union(const void *ptr, int e, void *buf,
		       size_t nbytes, int nmemb, ...) {
  const uint8_t *ptr_ = (const uint8_t *) ptr;
  va_list ap;

  va_start(ap, nmemb);

  for (int i = 0; i < nmemb; ++i) {
    size_t memboff = va_arg(ap, size_t);
    int membenum = va_arg(ap, int);
    serialize_f serialfn = va_arg(ap, serialize_f);

    if (membenum == e) {
      /* serialize this member */
      const uint8_t *membptr = ptr_ + memboff;
      return serialfn(membptr, buf, nbytes);
    }
  }

  /* no member matched this enum, so don't serialize anything */
  return 0;
}

size_t deserialize_union(const void *buf, size_t nbytes, void *ptr,
			 int e, int *errp, int nmemb, ...) {
  uint8_t *ptr_ = (uint8_t *) ptr;
  va_list ap;
  
  va_start(ap, nmemb);

  for (int i = 0; i < nmemb; ++i) {
    size_t memboff = va_arg(ap, size_t);
    int membenum = va_arg(ap, int);
    deserialize_f deserialfn = va_arg(ap, deserialize_f);

    if (membenum == e) {
      /* deserialize this member */
      uint8_t *membptr = ptr_ + memboff;
      return deserialfn(buf, nbytes, membptr, errp);
    }
  }

  /* no member matched this enum, so don't deserialize anything */
  return 0;
}


size_t serialize_enum(int *enump, void *buf,
		      size_t nbytes) {
  if (*enump >= 256 || *enump < 0) {
    /* enums with more than 256 items not supported */
    abort();
  }

  /* write single byte to buffer */
  if (nbytes >= 1) {
    *((uint8_t *) buf) = (uint8_t) *enump;
  }

  return 1; /* require exactly 1 byte */
}

size_t deserialize_enum(const void *buf, size_t nbytes,
			int *enump, int *errp) {

  if (*errp) {
    return 0;
  }
  
  if (nbytes >= 1) {
    *enump = *((uint8_t *) buf);
  }

  return 1; /* require exactly 1 byte */
}


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
  const uint8_t *buf_ = (const uint8_t *) buf;
  size_t len;
  
  /* bail on previous error */
  if (*errp) {
    return 0;
  }
  
  len = strnlen((const char *) buf_, nbytes);

  /* allocate string */
  if (len < nbytes) {
    char *str = strdup((const char *) buf_);
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

size_t serialize_uint32(const uint32_t *uint, void *buf,
			size_t nbytes) {
  /* TODO: write serialize_uint deserialize_uint */
  
  if (sizeof(uint32_t) <= nbytes) {
    *((uint32_t *) buf) = htonl(*uint);  
  }
  
  return sizeof(uint32_t);
}

size_t deserialize_uint32(const void *buf, size_t nbytes,
			  uint32_t *uint, int *errp) {
  /* bail on previous error */
  if (*errp) {
    return 0;
  }

  if (sizeof(uint32_t) <= nbytes) {
    *uint = ntohl(*((const uint32_t *) buf));
  }
  
  return sizeof(uint32_t);
}

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes) { 
  return
    serialize_struct((const void *) rsrc, buf, nbytes, 2,
		     offsetof(struct rsrc, mr_owner), serialize_strp,
		     offsetof(struct rsrc, mr_path), serialize_strp
		     );
}


size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp) {

#if 1

  return
    deserialize_struct(buf, nbytes, rsrc, errp, 2,
		       offsetof(struct rsrc,
				mr_owner), deserialize_str,
		       offsetof(struct rsrc,
				mr_path), deserialize_str
		       );
		       
			    

  
#else
  
  const uint8_t *buf_ = (const void *) buf;
  size_t used = 0;
  
  /* bail on previous error */
  if (*errp) {
    return 0;
  }
  
  used += deserialize_str(buf_ + used, sizerem(nbytes, used),
			  &rsrc->mr_owner, errp);
  used += deserialize_str(buf_ + used, sizerem(nbytes, used),
			  &rsrc->mr_path, errp);
  
  return *errp ? 0 : used;

#endif
}

		   


/* TODO -- serialization function for uint64_t */
size_t serialize_request(const struct middfs_request *req, void *buf,
			size_t nbytes) {
  uint8_t *buf_ = (uint8_t *) buf;
  size_t used = 0;

  /* TODO: write serialize_uint deserialize_uint */
  used += serialize_enum((int *) &req->mreq_type, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_str(req->mreq_requester, buf_ + used,
			sizerem(nbytes, used));
  used += serialize_rsrc(&req->rsrc, buf_ + used,
			 sizerem(nbytes, used));
  
  return used;
}

size_t deserialize_request(const void *buf, size_t nbytes,
			   struct middfs_request *req, int *errp) {
  const uint8_t *buf_ = (const uint8_t *) buf;
  size_t used = 0;

  /* bail on previous error */
  if (*errp) {
    return 0;
  }

  /* TODO: write serialize_uint deserialize_uint */
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
			     (uint32_t *) &req->mreq_type, errp);
  used += deserialize_str(buf_ + used, sizerem(nbytes, used),
			  &req->mreq_requester, errp);
  used += deserialize_rsrc(buf_ + used, sizerem(nbytes, used),
			   &req->rsrc, errp);

  return *errp ? 0 : used;
}

size_t serialize_pkt(const struct middfs_packet *pkt, void *buf,
			size_t nbytes) {
  uint8_t *buf_ = (uint8_t *) buf;
  size_t used = 0;
  
  used += serialize_uint32(&pkt->mpkt_magic, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_enum((int *) &pkt->mpkt_type, buf_ + used,
			   sizerem(nbytes, used));
  
  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
    used += serialize_request(&pkt->mpkt_request, buf_ + used,
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
  const uint8_t *buf_ = (const void *) buf;
  size_t used = 0;
  
  /* bail on previous error */
  if (*errp) {
    return 0;
  }
  
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
			     &pkt->mpkt_magic, errp);
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
			     (uint32_t *) &pkt->mpkt_type, errp);
  
  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
    used += deserialize_request(buf_ + used, sizerem(nbytes, used),
				&pkt->mpkt_request, errp);
  case MPKT_CONNECT:
  case MPKT_DISCONNECT:
  case MPKT_NONE:  
  default:
    abort();
  }

  return *errp ? 0 : used;
}



size_t serialize_uint64(const uint64_t *uint, void *buf,
			size_t nbytes) {
  /* serialize top 4 bytes, then serialize bottom 4 bytes */
  uint8_t *buf_ = (uint8_t *) buf;
  
  size_t used = 0;
  uint32_t uint1, uint2;
  uint1 = *uint >> 32;
  uint2 = *uint & 0xffffffff;
  
  used += serialize_uint32(&uint1, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_uint32(&uint2, buf_ + used,
			   sizerem(nbytes, used));
  
  return used;
}

