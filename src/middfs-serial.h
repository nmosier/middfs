/* middfs-serial.h -- serialization & deserialization functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERIAL_H
#define __MIDDFS_SERIAL_H

size_t serialize_str(const char *str, void *buf, size_t nbytes);

size_t deserialize_str(const void *buf, size_t nbytes,
		       char **strp, int *errp);

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes);

size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp);

#endif
