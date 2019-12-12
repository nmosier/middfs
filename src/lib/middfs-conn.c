/* middfs-server-conn.c -- middfs server connection manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <assert.h>

#include "lib/middfs-conn.h"
#include "lib/middfs-rsrc.h"
#include "lib/middfs-serial.h"
#include "lib/middfs-handler.h"

static enum handler_e handle_pkt_rd(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);
static enum handler_e handle_pkt_wr(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);

/* server_start() -- start the server on port _port_ 
   with backlog _backlog_.
   * RETV: the server socket on success, -1 on error.
   */
int server_start(const char *port, int backlog, struct middfs_socks *socks) {
  int servsock_fd = -1;
  struct addrinfo *res = NULL;
  int gai_stat;
  int error = 0;

  /* obtain socket */
  if ((servsock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("server_start: socket");
    return -1;
  }

  /* reuse the bloody address! */
  int reuseaddr = 1;
  if (setsockopt(servsock_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int)) < 0) {
    perror("setsockopt");
    return -1;
  }

  /* get address info */
  struct addrinfo hints = {0};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  if ((gai_stat = getaddrinfo(NULL, port, &hints, &res)) != 0) {
    /* getaddrinfo() error */
    fprintf(stderr, "server_start: getaddrinfo: %s\n",
	    gai_strerror(gai_stat));
    error = 1;
    goto cleanup;
  }
   
  /* bind socket to port */
  if (bind(servsock_fd, res->ai_addr, res->ai_addrlen) < 0) {
    perror("server_start: bind");
    error = 1;
    goto cleanup;
  }

  /* listen for connections */
  if (listen(servsock_fd, backlog) < 0) {
    perror("listen");
    error = 1;
    goto cleanup;
  }

  /* initialize socket list */
  /* assume _socks_ is already initialized */
  struct middfs_sockinfo serv_sockinfo;
  middfs_sockinfo_init(MFD_LSTN, servsock_fd, -1, &serv_sockinfo);
  if (middfs_socks_add(&serv_sockinfo, socks) < 0) {
    error = 1;
    middfs_sockinfo_delete(&serv_sockinfo);
    middfs_socks_delete(socks);
    goto cleanup;
  }

 cleanup:
  /* close server socket (if error occurred) */
  if (error && servsock_fd >= 0 && close(servsock_fd) < 0) {
    perror("close");
  }

  /* free res addrinfo (unconditionally) */
  if (res) {
    freeaddrinfo(res);
  }

  /* return -1 on error, 0 on success */
  return -error;
}


/* server_accept
 * DESC: accept client connection.
 * ARGS:
 *  - servfd: server socket listening for connections.
 * RETV: see accept(2).
 * NOTE: blocks.
 */
int server_accept(int servfd) {
  socklen_t addrlen;
  struct sockaddr_in client_sa;
  int client_fd;

  /* accept socket */
  addrlen = sizeof(client_sa);
  if ((client_fd = accept(servfd, (struct sockaddr *) &client_sa,
			  &addrlen)) < 0) {
    perror("server_accept: accept");
  }

  return client_fd;
}


int server_loop(struct middfs_socks *socks, const struct handler_info *hi) {
  int retv = 0;

  int readyfds;

  if ((readyfds = middfs_socks_poll(socks)) < 0) {
    perror("middfs_socks_poll");
    return -1;
  }

  int count = socks->count;
  for (int index = 0; index < count; ++index) {

     struct middfs_sockinfo new_sockinfo;
     enum handler_e status = handle_socket_event(&socks->sockinfos[index], hi, &new_sockinfo);
     
     switch (status) {
     case HS_SUC:
        break;
      
     case HS_NEW: /* Add new socket to list */
        if (middfs_socks_add(&new_sockinfo, socks) < 0) {
           perror("middfs_socks_add");
           if (middfs_sockinfo_delete(&new_sockinfo) < 0) {
              perror("middfs_sockinfo_delete");
           }
           return -1;
        }
        break;
      
     case HS_DEL: /* Remove socket */
        if (middfs_socks_remove(index, socks) < 0) {
           perror("middfs_socks_remove");
           return -1;
        }
        break;
      
     case HS_ERR:
     default:
        perror("handle_socket_event");
        return -1;
     }
    
     // fprintf(stderr, "nopen=%d\n", middfs_socks_pack(socks));
  }
  
  return retv;
  /* TODO: perhaps this should return the number of open 
   * sockets -- if this ever reaches 0, then the server should shut
   * down. */
}


/* RETV:
 *   (-1) indicates error and socket should be deleted.
 *   ( 0) indicates success and nothing else should be done.
 *   ( 1) indicates a new entry in the socket array should be created with _new_sockinfo_ parameter.
 */
enum handler_e handle_socket_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				   struct middfs_sockinfo *new_sockinfo) {

   if (middfs_sockinfo_isopen(sockinfo)) {
      switch (sockinfo->type) {
      case MFD_PKT_IN:
	return handle_pkt_event(sockinfo, hi);
         
      case MFD_PKT_OUT:
         abort(); /* TODO */
        
      case MFD_LSTN: /* POLLIN -- accept new client connection */
         return handle_lstn_event(sockinfo, hi, new_sockinfo);
    
      case MFD_NONE:
      default:
         abort();
      }
   }

  return 0;  
}


enum handler_e handle_lstn_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				 struct middfs_sockinfo *new_sockinfo) {
  int revents = sockinfo->revents;
  int listenfd = sockinfo->in.fd;
  int clientfd;

  if (revents & POLLIN) {
    if ((clientfd = server_accept(listenfd)) < 0) {
      perror("server_accept");
      return HS_DEL; /* delete listening socket */
    }

    middfs_sockinfo_init(MFD_PKT_IN, clientfd, -1, new_sockinfo);
    return HS_NEW; /* create new socket entry */
  }

  return HS_SUC;
}

enum handler_e handle_pkt_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi) {
  int revents = sockinfo->revents;

  if (revents) {
    switch (sockinfo->state) {
    case MSS_REQRD:
    case MSS_RSPFWD:
      return handle_pkt_rd(sockinfo, hi);
      
    case MSS_RSPWR:
    case MSS_REQFWD:
      return handle_pkt_wr(sockinfo, hi);

    case MSS_LSTN:
    case MSS_CLOSED:
    case MSS_NONE:
    default:
      abort();
    }
  }
  
  return 0;
}




static enum handler_e handle_pkt_rd(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi) {
  struct middfs_sockend *in = &sockinfo->in;
  int fd = in->fd;
  struct buffer *buf_in = &in->buf;
  ssize_t bytes_read;

  assert(sockinfo->revents & POLLIN);

  /* read bytes into buffer */
  bytes_read = buffer_read(fd, buf_in);
  if (bytes_read < 0) {
    perror("buffer_read");
    return HS_DEL; /* delete socket */
  }
  if (bytes_read == 0) {
     /* data ended prematurely -- remove socket from list */
     fprintf(stderr, "warning: data ended prematurely for socket %d\n", fd);
     return HS_DEL;
  }

  struct middfs_packet in_pkt = {0};
  int errp = 0;
  size_t bytes_ready = buffer_used(buf_in);
  size_t bytes_required = deserialize_pkt(buf_in->begin, bytes_ready, &in_pkt, &errp);
  
  if (errp) {
    /* invalid data; close socket */
    fprintf(stderr, "warning: packet from socket %d contains invalid data\n", fd);
    return HS_DEL;
  }

  if (bytes_required > bytes_ready) {
    return HS_SUC; /* wait on more bytes */
  }

  /* incoming packet has been successfully deserialized */

  /* remove used bytes */
  buffer_shift(buf_in, bytes_required);

  /* Call server/client-specific handler function to handle received data and determine
   * next socket state. */
  return hi->rd_fin(sockinfo, &in_pkt);
}


/* shared aux fn for rspwr and reqfwd */
static enum handler_e handle_pkt_wr(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi) {
  int fd = sockinfo->out.fd;
  struct buffer *buf_out = &sockinfo->out.buf;

  assert(sockinfo->revents & POLLOUT);
  
  ssize_t bytes_written = buffer_write(fd, buf_out);
  if (bytes_written < 0) {
    /* error */
    perror("buffer_write");
    return HS_DEL;
  }

  /* successful write, but more bytes to write, so don't change state */
  if (bytes_written > 0) {
     return HS_SUC;
  }
  
  /* all of bytes written;
   * Call server/client-specific handler function to determine next state.
   */
  return hi->wr_fin(sockinfo);
}
