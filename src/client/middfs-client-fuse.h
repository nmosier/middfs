/* middfs-client-fuse.h -- include this instead of FUSE headers */

#ifndef __MIDDFS_CLIENT_FUSE_H
#define __MIDDFS_CLIENT_FUSE_H

#ifndef FUSE
  #error "FUSE version is not defined."
#endif


#ifndef FUSE_USE_VERSION
  #if FUSE == 3
    #define FUSE_USE_VERSION 31
  #elif FUSE == 2
    #define FUSE_USE_VERSION 29
  #else
    #error "invalid fuse version"
  #endif
#endif

#include <fuse.h>

#endif
