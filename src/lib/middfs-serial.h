/* middfs-serial.h -- serialization & deserialization functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERIAL_H
#define __MIDDFS_SERIAL_H

#include <stdarg.h>

#include "middfs-util.h"
#include "middfs-serial.h"
#include "middfs-rsrc.h"
#include "middfs-pkt.h"

/* (De)serialization function prototypes */
typedef size_t (*serialize_f)(const void *ptr, void *buf,
			      size_t nbytes);
typedef size_t (*deserialize_f)(const void *buf, size_t nbytes,
				void *ptr, int *errp);


struct union_member {
  int enum_val; /* value of corresponding enum */
  serialize_f serialize; /* serialization function */
  deserialize_f deserialize; /* deserialization function */
  /* NOTE: More function pointers will probably be added. */
};

struct union_info {
  int nmemb;
  struct union_member membs[];
};



size_t deserialize_struct(const void *buf, size_t nbytes, void *ptr,
			  int *errp, int nmemb, ...);

size_t serialize_struct(const void *ptr, void *buf, size_t nbytes,
			int nmemb, ...);

size_t serialize_union(const void *ptr, int e, void *buf,
		       size_t nbytes, int nmemb, ...);

size_t deserialize_union(const void *buf, size_t nbytes, void *ptr,
			 int e, int *errp, int nmemb, ...);


size_t serialize_str(const char *str, void *buf, size_t nbytes);
size_t serialize_strp(const char **str, void *buf, size_t nbytes);

size_t deserialize_str(const void *buf, size_t nbytes,
		       char **strp, int *errp);

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes);

size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp);

size_t serialize_uint32(const uint32_t uint, void *buf,
			size_t nbytes);

size_t serialize_uint64(const uint64_t uint, void *buf,
			size_t nbytes);


size_t deserialize_uint32(const void *buf, size_t nbytes,
			  uint32_t *uint, int *errp);

size_t deserialize_int64(const void *buf, size_t nbytes,
			 int64_t *int64, int *errp);

size_t deserialize_uint64(const void *buf, size_t nbytes,
			  uint64_t *uint, int *errp);

size_t serialize_int64(const int64_t int64, void *buf,
		       size_t nbytes);

size_t serialize_int32(const int32_t int32, void *buf,
		       size_t nbytes);

size_t deserialize_int32(const void *buf, size_t nbytes,
			 int32_t *int32, int *errp);


size_t serialize_request(const struct middfs_request *req, void *buf,
			 size_t nbytes);

size_t deserialize_request(const void *buf, size_t nbytes,
			   struct middfs_request *req, int *errp);

size_t serialize_pkt(const struct middfs_packet *pkt, void *buf,
		     size_t nbytes);
  
size_t deserialize_pkt(const void *buf, size_t nbytes,
		       struct middfs_packet *pkt, int *errp);

size_t serialize_enum(int *e, void *buf,
		      size_t nbytes);

size_t deserialize_enum(const void *buf, size_t nbytes,
			int *enump, int *errp);

size_t serialize_rsp(const struct middfs_response *rsp, void *buf, size_t nbytes);
size_t deserialize_rsp(const void *buf, size_t nbytes, struct middfs_response *rsp, int *errp);

#endif
