/* middfs-sock.h -- shared middfs socket-related code
 * Nicholas Mosier & Tommaso Monaco
 */

#ifndef __MIDDFS_SOCK_H
#define __MIDDFS_SOCK_H

#include <poll.h>

/* middfs_fd_e -- enum describing type of socket */
enum middfs_socktype
  {MFD_NONE, /* None */
   MFD_CREQ, /* Client Request (client -> server) */
   MFD_SREQ, /* Server Request (server -> client) */
   MFD_LSTN, /* Listening Socket (unique) */
  };

/* struct middfs_sockinfo -- information about socket
 * NOTE: currently contains singleton member, but included
 * for future extendability. */
struct middfs_sockinfo {
  enum middfs_socktype type;
  /* Might need to add:
   *  - buffer pointer for incoming data
   *  - buffer pointer for outgoing data
   */
};

struct middfs_socks {
  struct pollfd *pollfds;
  struct middfs_sockinfo *sockinfos;
  nfds_t count;
  nfds_t len;
  nfds_t nopen; /* TODO: Delete this -- not used. */
};

int middfs_sockinfo_init(enum middfs_socktype type,
			 struct middfs_sockinfo *info);
int middfs_sockinfo_delete(struct middfs_sockinfo *info);


int middfs_socks_init(struct middfs_socks *socks);
int middfs_socks_delete(struct middfs_socks *socks);
int middfs_socks_resize(nfds_t newlen, struct middfs_socks *socks);
int middfs_socks_add(int sockfd, const struct middfs_sockinfo *sockinfo,
                     struct middfs_socks *socks);
int middfs_socks_remove(nfds_t index, struct middfs_socks *socks);
int middfs_socks_pack(struct middfs_socks *socks);


#endif
