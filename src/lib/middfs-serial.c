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

size_t serialize_uint32(const uint32_t uint, void *buf,
			size_t nbytes) {
  /* TODO: write serialize_uint deserialize_uint */
  
  if (sizeof(uint) <= nbytes) {
    *((uint32_t *) buf) = htonl(uint);  
  }
  
  return sizeof(uint);
}

size_t serialize_int32(const int32_t int32, void *buf,
		       size_t nbytes) {
  return serialize_uint32((const uint32_t) int32, buf,
			  nbytes);
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

size_t deserialize_int32(const void *buf, size_t nbytes,
			  int32_t *int32, int *errp) {
  return deserialize_uint32(buf, nbytes, (uint32_t *) int32,
			    errp);
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

size_t serialize_request(const struct middfs_request *req, void *buf,
			 size_t nbytes) {
  uint8_t *buf_ = (uint8_t *) buf;
  size_t used = 0;
  enum middfs_request_type type = req->mreq_type;

  /* serialize shared members */
  used += serialize_uint32((uint32_t) type, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_str(req->mreq_requester, buf_ + used,
			sizerem(nbytes, used));
  used += serialize_rsrc(&req->mreq_rsrc, buf_ + used,
			 sizerem(nbytes, used));


  /* serialize request-specific members */
  
  /* serialize _mode_ */
  if (req_has_mode(type)) {
     used += serialize_int32((int32_t) req->mreq_mode, buf_ + used, sizerem(nbytes, used));
  }

  /* serialize _size_ */
  if (req_has_size(type)) {
     used += serialize_uint64((int32_t) req->mreq_size, buf_ + used, sizerem(nbytes, used));
  }

  /* serialize _to_ */
  if (req_has_to(type)) {
    used += serialize_rsrc(&req->mreq_to, buf_ + used, sizerem(nbytes, used));
  }

  /* serilaize _off_ */
  if (req_has_off(type)) {
     used += serialize_uint64((int32_t) req->mreq_off, buf_ + used, sizerem(nbytes, used));
  }

  /* serialize _data_ */
  if (req_has_data(type)) {
     if (sizerem(nbytes, used) >= req->mreq_size) {
        memcpy(buf_ + used, req->mreq_data, req->mreq_size);
     }
     used += req->mreq_size;
  }
  
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

  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
			     (uint32_t *) &req->mreq_type, errp);
  used += deserialize_str(buf_ + used, sizerem(nbytes, used),
			  &req->mreq_requester, errp);
  used += deserialize_rsrc(buf_ + used, sizerem(nbytes, used),
			   &req->mreq_rsrc, errp);

  /* stop if already exceeded allowance */
  if (used > nbytes) {
    return used;
  }
  
  /* deserialize request-specific fields */
  enum middfs_request_type type = req->mreq_type;
  if (req_has_mode(type)) {
    used += deserialize_int32(buf_ + used, sizerem(nbytes, used), &req->mreq_mode, errp);
  }
  if (req_has_size(type)) {
     used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &req->mreq_size, errp);
  }
  if (req_has_to(type)) {
    used += deserialize_rsrc(buf_ + used, sizerem(nbytes, used), &req->mreq_to, errp);
  }
  if (req_has_off(type)) {
    used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &req->mreq_off, errp);
  }

  /* stop if already exceeded allowance */
  if (used > nbytes) {
     return used;
  }
  
  if (req_has_data(type)) {
     if (sizerem(nbytes, used) >= req->mreq_size) {
        if ((req->mreq_data = memdup(buf_ + used, req->mreq_size)) == NULL) {
           *errp = 1;
           return 0;
        }
     }
     used += req->mreq_size;
  }

  return *errp ? 0 : used;
}

size_t serialize_pkt(const struct middfs_packet *pkt, void *buf,
			size_t nbytes) {
  uint8_t *buf_ = (uint8_t *) buf;
  size_t used = 0;

  used += serialize_uint32((uint32_t) pkt->mpkt_magic, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_uint32((uint32_t) pkt->mpkt_type, buf_ + used,
			   sizerem(nbytes, used));

  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
     used += serialize_request(&pkt->mpkt_un.mpkt_request,
                               buf_ + used, sizerem(nbytes, used));
    break;

  case MPKT_RESPONSE:
     used += serialize_rsp(&pkt->mpkt_un.mpkt_response, buf_ + used, sizerem(nbytes, used));
     break;
    
  case MPKT_CONNECT:
     used += serialize_connect(&pkt->mpkt_un.mpkt_connect, buf_ + used, sizerem(nbytes, used));
     break;
     
  case MPKT_DISCONNECT:
  case MPKT_NONE:  
  default:
    /* TODO -- there aren't any fields to deserialize yet. */
    break;
  }
 
  return used;

}


size_t deserialize_pkt(const void *buf, size_t nbytes,
		       struct middfs_packet *pkt, int *errp) {
  const uint8_t *buf_ = (const void *) buf;
  size_t used = 0;
  
  if (*errp) {
    return 0;
  }
  
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
                             &pkt->mpkt_magic, errp);
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used),
                             (uint32_t *) &pkt->mpkt_type, errp);

  if (nbytes < used || *errp) {
     return used;
  }
  
  switch (pkt->mpkt_type) {
  case MPKT_REQUEST:
     used += deserialize_request(buf_ + used, sizerem(nbytes, used),
                                 &pkt->mpkt_un.mpkt_request, errp);
     break;

  case MPKT_RESPONSE:
     used += deserialize_rsp(buf_ + used, sizerem(nbytes, used),
                             &pkt->mpkt_un.mpkt_response, errp);
     break;
    
  case MPKT_CONNECT:
    used += deserialize_connect(buf_ + used, sizerem(nbytes, used),
				&pkt->mpkt_un.mpkt_connect, errp);
    break;
    
  case MPKT_DISCONNECT:
  case MPKT_NONE:  
  default:
     /* TODO -- there aren't any fields to deserialize yet. */
     break;
  }

  if (*errp) {
     return 0;
  }

  return used;
}



size_t serialize_uint64(const uint64_t uint, void *buf,
			size_t nbytes) {
  /* serialize top 4 bytes, then serialize bottom 4 bytes */
  uint8_t *buf_ = (uint8_t *) buf;
  
  size_t used = 0;
  uint32_t uint1, uint2;
  uint1 = uint >> 32;
  uint2 = uint & 0xffffffff;
  
  used += serialize_uint32(uint1, buf_ + used,
			   sizerem(nbytes, used));
  used += serialize_uint32(uint2, buf_ + used,
			   sizerem(nbytes, used));
  
  return used;
}

size_t serialize_int64(const int64_t int64, void *buf,
		       size_t nbytes) {
   return serialize_uint64((const uint64_t) int64, buf, nbytes);
}

size_t deserialize_uint64(const void *buf, size_t nbytes,
			  uint64_t *uint, int *errp) {
  /* deserialize top 4 bytes, then deserialize bottom 4 bytes */
  uint8_t *buf_ = (uint8_t *) buf;

  size_t used = 0;
  uint32_t uint1, uint2;

  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used), &uint1, errp);
  used += deserialize_uint32(buf_ + used, sizerem(nbytes, used), &uint2, errp);

  if (used <= nbytes) {
    uint64_t val = (((uint64_t) uint1) << 32) + ((uint64_t) uint2);
    *uint = val;
  }
  
  return used;
}

size_t deserialize_int64(const void *buf, size_t nbytes,
			 int64_t *int64, int *errp) {
  return deserialize_uint64(buf, nbytes, (uint64_t *) int64, errp);
}


size_t serialize_rsp(const struct middfs_response *rsp, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;

   /* serialize type first */
   used += serialize_int32(rsp->mrsp_type, buf_ + used, sizerem(nbytes, used));

   switch (rsp->mrsp_type) {
   case MRSP_OK:
      /* nothing to serialize */
      break;
      
   case MRSP_DATA:
      used += serialize_data(&rsp->mrsp_un.mrsp_data, buf_ + used, sizerem(nbytes, used));
      break;
      
   case MRSP_STAT:
      /* serialize stat */
      used += serialize_stat(&rsp->mrsp_un.mrsp_stat, buf_ + used, sizerem(nbytes, used));
      break;

   case MRSP_DIR:
      /* serialize dir */
      used += serialize_dir(&rsp->mrsp_un.mrsp_dir, buf_ + used, sizerem(nbytes, used));
      break;
      
   case MRSP_ERROR:
      /* serialize error */
      used += serialize_int32(rsp->mrsp_un.mrsp_error, buf_ + used, sizerem(nbytes, used));
      break;
      
   default:
      abort();
   }
   
   return used;
}

size_t deserialize_rsp(const void *buf, size_t nbytes, struct middfs_response *rsp, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   /* deserialize type */
   used += deserialize_int32(buf_ + used, sizerem(nbytes, used), (int32_t *) &rsp->mrsp_type, errp);

   /* return if couldn't deserialize type */
   if (used > nbytes) {
      return used;
   }

   switch (rsp->mrsp_type) {
   case MRSP_OK:
      /* nothing to deserialize */
      break;

   case MRSP_DATA:
      used += deserialize_data(buf_ + used, sizerem(nbytes, used), &rsp->mrsp_un.mrsp_data, errp);
      break;

   case MRSP_STAT:
      /* deserialize stat */
      used += deserialize_stat(buf_ + used, sizerem(nbytes, used), &rsp->mrsp_un.mrsp_stat, errp);
      break;

   case MRSP_DIR:
      /* deserialize dir */
      used += deserialize_dir(buf_ + used, sizerem(nbytes, used), &rsp->mrsp_un.mrsp_dir, errp);
      break;      
      
   case MRSP_ERROR:
      used += deserialize_int32(buf_ + used, sizerem(nbytes, used), &rsp->mrsp_un.mrsp_error, errp);
      break;
      
   default:
      abort();
   }

   return used;
}

size_t serialize_connect(const struct middfs_connect *conn, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;

   used += serialize_uint32(conn->port, buf_ + used, sizerem(nbytes, used));
   used += serialize_str(conn->name, buf_ + used, sizerem(nbytes, used));
   
   return used;
}

size_t deserialize_connect(const void *buf, size_t nbytes, struct middfs_connect *conn, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   used += deserialize_uint32(buf_ + used, sizerem(nbytes, used), &conn->port, errp);
   used += deserialize_str(buf_ + used, sizerem(nbytes, used), &conn->name, errp);

   return used;
}

size_t serialize_stat(const struct middfs_stat *st, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;
   
   used += serialize_uint32(st->mstat_mode, buf_ + used, sizerem(nbytes, used));
   used += serialize_uint64(st->mstat_size, buf_ + used, sizerem(nbytes, used));
   used += serialize_uint64(st->mstat_blocks, buf_ + used, sizerem(nbytes, used));
   used += serialize_uint64(st->mstat_blksize, buf_ + used, sizerem(nbytes, used));
   
   return used;
}

size_t deserialize_stat(const void *buf, size_t nbytes, struct middfs_stat *st, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   used += deserialize_uint32(buf_ + used, sizerem(nbytes, used), &st->mstat_mode, errp);
   used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &st->mstat_size, errp);
   used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &st->mstat_blocks, errp);
   used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &st->mstat_blksize, errp);

   return used;
}

size_t serialize_data(const struct middfs_data *data, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;
   
   /* serialize data size */
   used += serialize_uint64(data->mdata_nbytes, buf_ + used, sizerem(nbytes, used));
   if (data->mdata_nbytes > 0 && data->mdata_nbytes <= sizerem(nbytes, used)) {
      /* serialize buffer */
      memcpy(buf_ + used, data->mdata_buf, data->mdata_nbytes);
   }
   used += data->mdata_nbytes;

   return used;
}

size_t deserialize_data(const void *buf, size_t nbytes, struct middfs_data *data, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   /* deserialize data */
   used += deserialize_uint64(buf_ + used, sizerem(nbytes, used),
                              &data->mdata_nbytes, errp);
   
   if (used > nbytes || *errp) {
      return used;
   }
   
   if (sizerem(nbytes, used) >= data->mdata_nbytes && data->mdata_nbytes > 0) {
      if ((data->mdata_buf = malloc(data->mdata_nbytes)) == NULL) {
         *errp = 1;
         return 0;
      }
      memcpy(data->mdata_buf, buf_ + used, data->mdata_nbytes);
   }
   used += data->mdata_nbytes;

   return used;
}

size_t serialize_dir(const struct middfs_dir *dir, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;

   /* serialize dirent count */
   used += serialize_uint64(dir->mdir_count, buf_ + used, sizerem(nbytes, used));

   /* serialize entries */
   for (uint64_t i = 0; i < dir->mdir_count; ++i) {
      used += serialize_dirent(&dir->mdir_ents[i], buf_ + used, sizerem(nbytes, used));
   }

   return used;
}

size_t deserialize_dir(const void *buf, size_t nbytes, struct middfs_dir *dir, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   used += deserialize_uint64(buf_ + used, sizerem(nbytes, used), &dir->mdir_count, errp);

   if (*errp || used > nbytes) {
      return used;
   }

   /* allocate dirent array */
   if (dir->mdir_ents == NULL) {
      if ((dir->mdir_ents = calloc(dir->mdir_count, sizeof(*dir->mdir_ents))) == NULL) {
         *errp = 1;
         return 0;
      }
   }

   for (uint64_t i = 0; i < dir->mdir_count; ++i) {
      used += deserialize_dirent(buf_ + used, sizerem(nbytes, used), &dir->mdir_ents[i], errp);
   }

   return used;
}

size_t serialize_dirent(const struct middfs_dirent *dirent, void *buf, size_t nbytes) {
   uint8_t *buf_ = (uint8_t *) buf;
   size_t used = 0;

   used += serialize_str(dirent->mde_name, buf_ + used, sizerem(nbytes, used));
   used += serialize_int32(dirent->mde_mode, buf_ + used, sizerem(nbytes, used));

   return used;
}

size_t deserialize_dirent(const void *buf, size_t nbytes, struct middfs_dirent *dirent, int *errp) {
   const uint8_t *buf_ = (const uint8_t *) buf;
   size_t used = 0;

   used += deserialize_str(buf_ + used, sizerem(nbytes, used), &dirent->mde_name, errp);
   used += deserialize_int32(buf_ + used, sizerem(nbytes, used), &dirent->mde_mode, errp);

   return used;
}
