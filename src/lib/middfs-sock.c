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

#include "middfs-util.h"
#include "middfs-sock.h"

/* middfs_socks_init() -- initialize the _socks_ struct for use
 * by other middfs_socks_* functions
 * ARGS:
 *  - socks: pointer to socket struct to be initialized 
 * RETV: 0 on success; -1 on error
 */
void middfs_socks_init(struct middfs_socks *socks) {
   socks->sockinfos = NULL;
   socks->count = 0;
   socks->len = 0;
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
    if (middfs_socks_remove(i, socks) < 0) {
      retv = -1;
    }
  }
  
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

  /* realloc sockinfos array */
  if ((ptr = realloc(socks->sockinfos,
		     newlen * sizeof(*socks->sockinfos))) == NULL) {
    return -1;
  }
  
  socks->sockinfos = ptr;
  socks->len = newlen;

  return 0;
}


int middfs_socks_add(const struct middfs_sockinfo *sockinfo, struct middfs_socks *socks) {
  /* resize if necessary */
  if (socks->count == socks->len) {
    nfds_t new_len = MAX(2 * socks->len, 16);
    if (middfs_socks_resize(new_len, socks) < 0) {
      return -1;
    }
  }
  
  socks->sockinfos[socks->count] = *sockinfo;
  ++socks->count;

  return 0;
}

int middfs_socks_remove(nfds_t index, struct middfs_socks *socks) {
  int retv = 0;
  
  assert(index < socks->count);

  if (middfs_sockinfo_delete(&socks->sockinfos[index]) < 0) {
     retv = -1;
  }

  return retv;
}

/* middfs_socks_pack() -- pack open sockets to beginning of array
 * ARGS:
 *  - socks: socket array
 * RETV: the number of open sockets in the array; -1 on error
 */
int middfs_socks_pack(struct middfs_socks *socks) {
  int nopen = 0;

  for (int closed_index = 0; closed_index < socks->count; ++closed_index) {

     if (!middfs_sockinfo_isopen(&socks->sockinfos[closed_index])) {

        for (int open_index = closed_index + 1; open_index < socks->count; ++open_index) {

           if (middfs_sockinfo_isopen(&socks->sockinfos[open_index])) {
              middfs_sockinfo_move(&socks->sockinfos[closed_index], &socks->sockinfos[open_index]);
              break;
           }
	
        }
    } else {
        
      ++nopen;
      
    }
     
  }

  socks->count = nopen;

  return nopen;
}


/* middfs_socks_pollfds() -- construct pollfd array for poll(2).
 * ARGS:
 *  - pfds: array of pollfd structs to populate. 
 *  - nfds: number of fds filled into _pdfs_.
 *  - socks: socket array to construct _pfds_ from. 
 * RETV: number of pollfds filled on success; -1 on error.
 * NOTE: Assumes distinctness of fds in _socks_.
 */
ssize_t middfs_socks_pollfds(struct pollfd *pfds, size_t nfds, struct middfs_socks *socks) {
  size_t nfds_used = 0;
  int err = 0;
  
  for (size_t i = 0; i < socks->count; ++i) {
    struct middfs_sockinfo *info = &socks->sockinfos[i];
    nfds_used += middfs_sockinfo_pollfds(pfds + nfds_used, sizerem(nfds, nfds_used), info, &err);
  }

  return err ? -1 : nfds_used;
}

/* middfs_socks_poll() -- poll on socket array.
 * ARGS:
 *  - socks: socket array to poll on.
 * RETV: see poll(2).
 */
int middfs_socks_poll(struct middfs_socks *socks) {
  int retv = -1;
  ssize_t nfds = -1;
  struct pollfd *pfds = NULL;

  /* determine size of pollfs array */
  if ((nfds = middfs_socks_pollfds(NULL, 0, socks)) < 0) {
    perror("middfs_socks_pollfds");
    goto cleanup;
  }

  /* allocate pollfd array */
  if ((pfds = calloc(nfds, sizeof(pfds[0]))) == NULL) {
    perror("calloc");
    goto cleanup;
  }

  /* populate pollfd array */
  middfs_socks_pollfds(pfds, nfds, socks);
  
  /* poll on array */
  if ((poll(pfds, nfds, -1)) < 0) {
    perror("poll");
    goto cleanup;
  }

  /* process all pollfd events */
  retv = middfs_socks_check(socks);

 cleanup:
  if (pfds != NULL) {
    free(pfds);
  }

  return retv;
}

/******************
 * SOCKINFO funcs *
 *****************/

int middfs_sockinfo_init(enum middfs_socktype type, int fd_in, int fd_out,
			 struct middfs_sockinfo *info) {
  info->type = type;
  switch (type) {
  case MFD_NONE:
     info->state = MSS_NONE;
     break;
  case MFD_PKT_IN:
     info->state = MSS_REQRD;
     break;
  case MFD_LSTN:
     info->state = MSS_LSTN;
     break;
  case MFD_PKT_OUT:
  default:
     abort();
  }
  middfs_sockend_init(fd_in, &info->in);
  middfs_sockend_init(fd_out, &info->out);
  return 0;
}

int middfs_sockinfo_delete(struct middfs_sockinfo *info) {
  info->type = MFD_NONE;
  middfs_sockend_delete(&info->in);
  middfs_sockend_delete(&info->out);
  return 0;
}

/* middfs_sockinfo_pollfds() -- initialize pollfd struct(s) for given socket. 
 * ARGS:
 *  - pfds: pollfds struct array, where 0-2 elements will be initialized
 *  - nfds: number of entries left in pollfds struct array
 *  - info: socket info struct.
 * RETV: number of pollfd structs used (or would have been used, if greater than
 *       nfds); -1 on error.
 * TODO: This needs further development.
 */
size_t middfs_sockinfo_pollfds(struct pollfd *pfds, size_t nfds, struct middfs_sockinfo *info,
			       int *errp) {
  size_t used = 0;

  /* bail on error */
  if (*errp != 0 || info->type == MFD_NONE) {
    return 0;
  }

  enum middfs_sockstate st = info->state;

  if (st == MSS_LSTN || st == MSS_REQRD || st == MSS_RSPFWD) {
     used += middfs_sockend_pollfd(&pfds[used], sizerem(nfds, used), POLLIN, &info->in, errp);
  }
  if (st == MSS_RSPWR || st == MSS_REQFWD) {
     used += middfs_sockend_pollfd(&pfds[used], sizerem(nfds, used), POLLOUT, &info->out, errp);
  }
  
  return used;
}

/* middfs_sockinfo_check() -- check given socket for any events after calling poll(2) 
 * ARGS:
 *  - pfds: pollfd array/pointer
 *  - nfds_checked: current index of pfds to look at. Updated by this routine.
 *  - info: socket info pointer.
 * RETV: returns POLLIN if socket is ready for reading; POLLOUT if ready for writing.
 *       -1 on error.
 * NOTE: This function updates _nfds_checked_. If two events occur on the socket's fds,
 *       then only one will be processed. The others will be caught with the next poll(2). 
 */
int middfs_sockinfo_check(struct middfs_sockinfo *info) {
   enum middfs_sockstate st = info->state;
   int revents_in = 0;
   int revents_out = 0;

   if (st == MSS_LSTN || st == MSS_REQRD || st == MSS_RSPFWD) {
      revents_in = middfs_sockend_check(&info->in);
   }
   if (st == MSS_RSPWR || st == MSS_REQFWD) {
      revents_out = middfs_sockend_check(&info->out);
   }
  
  if (revents_in < 0 || revents_out < 0) {
    return -1;
  } else {
    info->revents = revents_in | revents_out;
    return info->revents;
  }
}

/* middfs_socks_check() -- check revents status for socks array 
 * ARGS:
 *  - socks: socket list
 * RETV: returns number of sockets with events; -1 on error.
 */
int middfs_socks_check(struct middfs_socks *socks) {
  int nready = 0;

  for (int i = 0; i < socks->count; ++i) {
    int revents;
    if ((revents = middfs_sockinfo_check(&socks->sockinfos[i])) < 0) {
      middfs_socks_remove(i, socks); /* delete entry */
    } else if (revents > 0) {
      ++nready;
    }
  }

  return nready;
}

/*********************
 * SOCKEND FUNCTIONS *
 *********************/

void middfs_sockend_init(int fd, struct middfs_sockend *sockend) {
  sockend->fd = fd;
  buffer_init(&sockend->buf);
  sockend->revents = NULL;
}

int middfs_sockend_delete(struct middfs_sockend *sockend) {
  buffer_delete(&sockend->buf);
  if (sockend->fd >= 0) {
     int fd = sockend->fd;
     sockend->fd = -1;
     if (close(fd) < 0) {
        return -1;
     }
  }
  return 0;
}

int middfs_sockend_check(struct middfs_sockend *sockend) {
   int revents;
   int fd = sockend->fd;
   
  if (fd < 0) {
     return 0; /* ignore */
  }

  revents = *sockend->revents;  
  
  if (revents & POLLERR) {
     fprintf(stderr, "warning: error condition on socket %d\n", sockend->fd);
     return -1;
  } else if (revents & POLLHUP) {
     fprintf(stderr, "warning: socket %d disconnected\n", sockend->fd);
     return -1;
  } else {
     return revents;
  }
}

/* middfs_sockend_pollfd() -- create entry in pollfd array for sockend
 * ARGS:
 *  - pfds: pollfds array
 *  - nfds: number of elements in array 
 *  - polarity: either POLLIN or POLLOUT
 *  - sockend: pointer to sockend (in or out)
 *  - errp: error pointer
 * RETV: number of pollfd elements populated on success; 0 on error, and *errp set.
 * NOTE: This routine will add at most 1 pollfd to the array. */
int middfs_sockend_pollfd(struct pollfd *pfds, int nfds, int polarity,
			  struct middfs_sockend *sockend, int *errp) {
  int nfds_used = 0;
  
  if (*errp) {
    return 0;
  }

  assert(sockend->fd >= 0);

  nfds_used = 1;     
  if (nfds > 0) {
     pfds->fd = sockend->fd;
     pfds->events = polarity & (POLLIN | POLLOUT);
     sockend->revents = &pfds->revents;
  }
  
  return nfds_used;
}


bool middfs_sockend_isopen(const struct middfs_sockend *sockend) {
   return sockend->fd >= 0;
}

bool middfs_sockinfo_isopen(const struct middfs_sockinfo *sockinfo) {
   return middfs_sockend_isopen(&sockinfo->in) || middfs_sockend_isopen(&sockinfo->out);
}


void middfs_sockinfo_move(struct middfs_sockinfo *dst, struct middfs_sockinfo *src) {
   *dst = *src;
   if (dst != src) {
      middfs_sockinfo_init(MFD_NONE, -1, -1, src); /* initialize to ``closed'' sockinfo struct */
   }
}
