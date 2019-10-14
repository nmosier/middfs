/* middfs-sock.c -- shared code for socket handling 
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "middfs-sock.h"

/* middfs_socks_init() -- initialize the _socks_ struct for use
 * by other middfs_socks_* functions
 * ARGS:
 *  - socks: pointer to socket struct to be initialized 
 * RETV: 0 on success; -1 on error
 */
int middfs_socks_init(struct middfs_socks *socks) {
  memset(socks, sizeof(*socks), 0);
  if (middfs_socks_resize(1, socks) < 0) {
    return -1;
  }

  return 0;
}

/* middfs_socks_delete() -- destroy a socket list
 * ARGS:
 *  - socks: struct to destroy
 * RETV: 0 on success; -1 on error
 */
int middfs_socks_delete(struct middfs_socks *socks) {
  int retv = 0;

  /* close any open sockets */
  for (nfds_t i = 0; i < socks->count; ++i) {
    if (middfs_socks_remove(i, socks) < 0) {
      retv = -1;
    }
  }
  
  free(socks->pollfds);
  free(socks->sockinfos);
  
  return retv;
}

/* middfs_socks_resize() -- resize a middfs_socks struct
 * ARGS:
 *  - newlen: new number of sockets to accomodate
 *  - socks: struct to operate on
 * RETV: 0 on success; -1 on error
 */
int middfs_socks_resize(nfds_t newlen, struct middfs_socks *socks) {
  void *ptr;

  /* realloc pollfds array */
  if ((ptr = realloc(socks->pollfds,
		     newlen * sizeof(*socks->pollfds))) == NULL) {
    return -1;
  }
  socks->pollfds = ptr;

  /* realloc sockinfos array */
  if ((ptr = realloc(socks->pollfds,
		     newlen * sizeof(*socks->sockinfos))) == NULL) {
    return -1;
  }
  
  socks->sockinfos = ptr;
  socks->len = newlen;

  return 0;
}


int middfs_socks_add(int sockfd, struct middfs_sockinfo *sockinfo,
		     struct middfs_socks *socks) {

  /* resize if necessary */
  if (socks->count == socks->len) {
    nfds_t new_len = 2 * socks->len;
    
    if (middfs_socks_resize(new_len, socks) < 0) {
      return -1;
    }
  }

  struct pollfd *pollfd = &socks->pollfds[socks->count];
  pollfd->fd = sockfd;
  
  switch (sockinfo->type) {
  case MFD_CREQ:
  case MFD_SREQ:
    pollfd->events = POLLIN | POLLOUT;
    break;
    
  case MFD_LSTN:
    pollfd->events = POLLIN;
    break;

  case MFD_NONE:
  default:
    abort();
  }

  socks->sockinfos[socks->count] = *sockinfo;
  ++socks->count;

  if (sockfd >= 0) {
    ++socks->nopen;
  }

  return 0;
}

int middfs_socks_remove(nfds_t index, struct middfs_socks *socks) {
  int retv = 0;
  
  assert(index < socks->count);
  
  int *fdp = &socks->pollfds[index].fd;
  
  if (*fdp < 0) {
    errno = ENOTCONN;
    return -1;
  }
  
  if (close(*fdp) < 0) {
    return -1;
  }

  *fdp = -1; /* mark as deleted */
  if (middfs_sockinfo_delete(&socks->sockinfos[index]) < 0) {
    retv = -1;
  }

  /* update counts */
  --socks->nopen;
  --socks->count;

  return retv;
}



/******************
 * SOCKINFO funcs *
 *****************/

int middfs_sockinfo_init(enum middfs_socktype type,
			 struct middfs_sockinfo *info) {
  info->type = type;
  return 0;
}

int middfs_sockinfo_delete(struct middfs_sockinfo *info) {
  info->type = MFD_NONE;
  return 0;
}
