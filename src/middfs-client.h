/* middfs client file system
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#ifndef __MIDDFS_CLIENT_H
#define __MIDDFS_CLIENT_H

#define FUSE_USE_VERSION 31

#include <fuse.h>

struct middfs_conf {
  char *local_dir;
  char *client_name;
};

extern struct middfs_conf middfs_conf;

#endif