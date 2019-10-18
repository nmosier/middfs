/* middfs-serial.h -- serialization & deserialization functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERIAL_H
#define __MIDDFS_SERIAL_H

#include "middfs-util.h"
#include "middfs-serial.h"
#include "middfs-rsrc.h"
#include "middfs-pkt.h"

size_t serialize_str(const char *str, void *buf, size_t nbytes);

size_t deserialize_str(const void *buf, size_t nbytes,
		       char **strp, int *errp);

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes);

size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp);

size_t serialize_uint32(uint32_t uint, void *buf,
			size_t nbytes);

size_t deserialize_uint32(const void *buf, size_t nbytes,
			  uint32_t uint, int *errp);

size_t serialize_request(const struct middfs_request *req, void *buf,
			 size_t nbytes);

size_t deserialize_request(const void *buf, size_t nbytes,
			   struct middfs_request *req, int *errp);

size_t serialize_pkg(const struct middfs_packet *pkt, void *buf,
		     size_t nbytes);
  
size_t deserialize_pkg(const void *buf, size_t nbytes,
		       struct middfs_packet *pkt, int *errp);

#endif
