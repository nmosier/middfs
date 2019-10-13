/* middfs-sock.c -- shared code for socket handling 
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

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
  for (int i = 0; i < socks->count; ++i) {
    int sockfd;
    if ((sockfd = socks->pollfds[i].fd) >= 0) {
      if (close(sockfd) < 0) {
	perror("middfs_socks_delete: close");
	retv = -1;
      }
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
    perror("middfs_socks_resize: realloc");
    return -1;
  }
  socks->pollfds = ptr;

  /* realloc sockinfos array */
  if ((ptr = realloc(socks->pollfds,
		     newlen * sizeof(*socks->sockinfos))) == NULL) {
    perror("middfs_socks_resize: realloc");
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
    
  default:
    abort();
  }

  socks->sockinfos[socks->count] = *sockinfo;
  socks->count ++;

  return 0;
}
