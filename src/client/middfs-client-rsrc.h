/* middfs-client-client_rsrc.h -- interface for utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#ifndef __MIDDFS_CLIENT_UTIL_H
#define __MIDDFS_CLIENT_UTIL_H

#include <sys/stat.h>

#include "lib/middfs-rsrc.h"

#include "client/middfs-client-fuse.h"

struct client_rsrc {
  /* mr_type: type of resource (local or network)
   * MR_NETWORK: the file is located on the network 
   * MR_LOCAL: the file is located on this client machine 
   * MR_ROOT: resource is the middfs root directory ("/"). 
   */
  enum client_rsrc_type {MR_NETWORK, MR_LOCAL, MR_ROOT} mr_type;

  struct rsrc mr_rsrc;
  
  /* mr_fd: file descriptor (MR_LOCAL) or socket descriptor
   * (MR_NETWORK). Not used for MR_ROOT. Should be initialized
   * to -1.
   */
  int mr_fd;
};

char *middfs_localpath_tmp(const char *middfs_path);
int middfs_abspath(char **path);

int client_rsrc_create(const char *path, struct client_rsrc *client_rsrc);
int client_rsrc_delete(struct client_rsrc *client_rsrc);
int client_rsrc_open(struct client_rsrc *client_rsrc, int flags, ...);
int client_rsrc_lstat(const struct client_rsrc *client_rsrc,
		      struct stat *sb);
int client_rsrc_readlink(const struct client_rsrc *client_rsrc,
			 char *buf, size_t bufsize);
int client_rsrc_access(const struct client_rsrc *client_rsrc, int mode);
int client_rsrc_mkdir(const struct client_rsrc *client_rsrc, mode_t mode);
int client_rsrc_unlink(const struct client_rsrc *client_rsrc);
int client_rsrc_rmdir(const struct client_rsrc *client_rsrc);
int client_rsrc_truncate(const struct client_rsrc *client_rsrc, off_t size);
int client_rsrc_symlink(const struct client_rsrc *client_rsrc,
			const char *to);
int client_rsrc_rename(const struct client_rsrc *from,
		       const struct client_rsrc *to);
int client_rsrc_chmod(const struct client_rsrc *client_rsrc, mode_t mode);

int client_rsrc_read(const struct client_rsrc *client_rsrc, char *buf, size_t size, off_t offset);
int client_rsrc_write(const struct client_rsrc *client_rsrc, const void *buf,
                      size_t size, off_t offset);
int client_rsrc_readdir(struct client_rsrc *rsrc, void *buf, fuse_fill_dir_t filler,
                        off_t offset);


#endif
