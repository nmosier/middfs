/* middfs-rsrc
 * Nicholas Mosier & Tommaso Monaco
 * 2019
 */

#ifndef __MIDDFS_RSRC_H
#define __MIDDFS_RSRC_H

struct rsrc {
  /* mr_owner: who owns this file, i.e. under which
   * middfs subdirectory it appears */
  char *mr_owner;

  /* mr_path: path of this file, relative to owner folder
   * NOTE: should still start with leading '/'. */
  char *mr_path;
};

size_t serialize_rsrc(const struct rsrc *rsrc, void *buf,
		      size_t nbytes);
size_t deserialize_rsrc(const void *buf, size_t nbytes,
			struct rsrc *rsrc, int *errp);


#endif
