/* middfs-sock.h -- shared middfs socket-related code
 * Nicholas Mosier & Tommaso Monaco
 */

#ifndef __MIDDFS_SOCK_H
#define __MIDDFS_SOCK_H

#include <poll.h>

#include "middfs-buf.h"

/* middfs_fd_e -- enum describing type of socket */
enum middfs_socktype
  {MFD_NONE, /* None */
   MFD_PKT_IN, /* Client Request (client -> server) */
   MFD_PKT_OUT, /* Server Request (server- > client) */
   MFD_LSTN, /* Listening Socket (unique) */
   MFD_NTYPES /* counts number of enum types */
  };

enum middfs_sockstate
  {MSS_NONE,
   MSS_CLOSED,
   MSS_LSTN,
   MSS_REQRD,
   MSS_RSPWR,
   MSS_REQFWD,
   MSS_RSPFWD,
   MSS_NTYPES
  };

struct middfs_sockend {
  int fd;
  struct buffer buf;

  /* Temporary Members */
  const short *revents; /* used by middfs_sockinfo_pollfd() */
};

/* struct middfs_sockinfo -- information about socket
 * NOTE: currently contains singleton member, but included
 * for future extendability. */
struct middfs_sockinfo {
  enum middfs_socktype type;
  enum middfs_sockstate state;

  struct middfs_sockend in;
  struct middfs_sockend out;

  int revents; /* combined revents mask */
};

struct middfs_socks {
  struct middfs_sockinfo *sockinfos;
  int count;
  int len;
};

int middfs_sockinfo_init(enum middfs_socktype type, int fd_in, int fd_out,
			 struct middfs_sockinfo *info);
int middfs_sockinfo_delete(struct middfs_sockinfo *info);
int middfs_sockinfo_checkfds(struct pollfd *pfds, int *nfds_checked,
			     struct middfs_sockinfo *info);

void middfs_socks_init(struct middfs_socks *socks);
int middfs_socks_delete(struct middfs_socks *socks);
int middfs_socks_resize(nfds_t newlen, struct middfs_socks *socks);
int middfs_socks_add(const struct middfs_sockinfo *sockinfo,
                     struct middfs_socks *socks);
int middfs_socks_remove(nfds_t index, struct middfs_socks *socks);
int middfs_socks_pack(struct middfs_socks *socks);



/*********************
 * SOCKEND FUNCTIONS *
 *********************/
void middfs_sockend_init(int fd, struct middfs_sockend *sockend);
int middfs_sockend_delete(struct middfs_sockend *sockend);

/* POLLING FUNCTIONS */

int middfs_socks_poll(struct middfs_socks *socks);

int middfs_sockend_pollfd(struct pollfd *pfds, int nfds, int polarity,
			  struct middfs_sockend *sockend, int *errp);
size_t middfs_sockinfo_pollfds(struct pollfd *pfds, size_t nfds, struct middfs_sockinfo *info,
			       int *errp);
ssize_t middfs_socks_pollfds(struct pollfd *pfds, size_t nfds, struct middfs_socks *socks);

/* Checking after polling */
int middfs_sockend_check(struct middfs_sockend *sockend);
int middfs_sockinfo_check(struct middfs_sockinfo *info);
int middfs_socks_check(struct middfs_socks *socks);


bool middfs_sockend_isopen(const struct middfs_sockend *sockend);
bool middfs_sockinfo_isopen(const struct middfs_sockinfo *sockinfo);

void middfs_sockinfo_move(struct middfs_sockinfo *dst, struct middfs_sockinfo *src);

#endif
