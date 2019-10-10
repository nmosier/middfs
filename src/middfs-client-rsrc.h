/* middfs-client-rsrc.h -- interface for utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#ifndef __MIDDFS_CLIENT_UTIL_H
#define __MIDDFS_CLIENT_UTIL_H

#include <sys/stat.h>

struct middfs_rsrc {
  /* mr_type: type of resource (local or network)
   * MR_NETWORK: the file is located on the network 
   * MR_LOCAL: the file is located on this client machine 
   * MR_ROOT: resource is the middfs root directory ("/"). 
   */
  enum middfs_rsrc_type {MR_NETWORK, MR_LOCAL, MR_ROOT} mr_type;

  /* mr_owner: who owns this file, i.e. under which
   * middfs subdirectory it appears */
  char *mr_owner;

  /* mr_path: path of this file, relative to owner folder
   * NOTE: should still start with leading '/'. */
  char *mr_path;

  /* mr_fd: file descriptor (MR_LOCAL) or socket descriptor
   * (MR_NETWORK). Not used for MR_ROOT. Should be initialized
   * to -1.
   */
  int mr_fd;
};

char *middfs_localpath(const char *path);
char *middfs_localpath_tmp(const char *middfs_path);
int middfs_abspath(char **path);

int middfs_rsrc_create(const char *path, struct middfs_rsrc *rsrc);
int middfs_rsrc_delete(struct middfs_rsrc *rsrc);
int middfs_rsrc_open(struct middfs_rsrc *rsrc, int flags, ...);
int middfs_rsrc_lstat(const struct middfs_rsrc *rsrc,
		      struct stat *sb);
int middfs_rsrc_readlink(const struct middfs_rsrc *rsrc,
			 char *buf, size_t bufsize);
int middfs_rsrc_access(const struct middfs_rsrc *rsrc, int mode);
int middfs_rsrc_mkdir(const struct middfs_rsrc *rsrc, mode_t mode);
int middfs_rsrc_unlink(const struct middfs_rsrc *rsrc);



#endif
