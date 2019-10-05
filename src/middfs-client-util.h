/* middfs-client-util.h -- interface for utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#ifndef __MIDDFS_CLIENT_UTIL_H
#define __MIDDFS_CLIENT_UTIL_H


struct middfs_rsrc {
  /* mr_type: type of resource (local or network)
   * MR_NETWORK: the file is located on the network 
   * MR_LOCAL: the file is located on this client machine 
   * MR_ROOT: resource is the middfs root directory ("/"). 
   */
  enum middfs_rsrc_type {MR_NETWORK, MR_LOCAL, MR_ROOT} mr_type;

  /* owner: who owns this file, i.e. under which
   * middfs subdirectory it appears */
  char *mr_owner;

  /* path: path of this file, relative to owner folder
   * NOTE: should still start with leading '/'. */
  char *mr_path;
};

char *middfs_localpath(const char *path);
int middfs_abspath(char **path);

int middfs_rsrc(const char *path, struct middfs_rsrc *rsrc);

#endif
