/* middfs-util.h -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_UTIL_H
#define __MIDDFS_UTIL_H

#include <string.h>

size_t serialize_str(const char *str, void *buf, size_t nbytes);
size_t deserialize_str(const void *buf, size_t nbytes,
		       char **strp, int *errp);
size_t sizerem(size_t nbytes, size_t used);

#endif
