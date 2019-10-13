/* middfs-sock.h -- shared middfs socket-related code
 * Nicholas Mosier & Tommaso Monaco
 */

#ifndef __MIDDFS_SOCK_H
#define __MIDDFS_SOCK_H

#include <poll.h>

/* middfs_fd_e -- enum describing type of socket */
enum middfs_socktype
  {MFD_CREQ, /* Client Request (client -> server) */
   MFD_SREQ, /* Server Request (server -> client) */
   MFD_LSTN, /* Listening Socket (unique) */
  };

/* struct middfs_sockinfo -- information about socket
 * NOTE: currently contains singleton member, but included
 * for future extendability. */
struct middfs_sockinfo {
  enum middfs_socktype type;
};

struct middfs_socks {
  struct pollfd *pollfds;
  struct middfs_sockinfo *sockinfos;
  nfds_t count;
};

#endif
