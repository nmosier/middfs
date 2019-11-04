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

#include "middfs-conn.h"
#include "middfs-rsrc.h"
#include "middfs-serial.h"
#include "middfs-handler.h"

static enum handler_e handle_pkt_wr(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);
static enum handler_e handle_pkt_rd(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);

/* server_start() -- start the server on port _port_ 
   with backlog _backlog_.
   * RETV: the server socket on success, -1 on error.
   */
int server_start(const char *port, int backlog) {
  int servsock_fd = -1;
  struct addrinfo *res = NULL;
  int gai_stat;
  int error = 0;

  /* obtain socket */
  if ((servsock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("server_start: socket");
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

 cleanup:
  /* close server socket (if error occurred) */
  if (error && servsock_fd >= 0 && close(servsock_fd) < 0) {
    perror("close");
  }

  /* free res addrinfo (unconditionally) */
  if (res) {
    freeaddrinfo(res);
  }

  /* return -1 on error, server socket on success */
  return error ? -1 : servsock_fd;
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
    
     fprintf(stderr, "nopen=%d\n", middfs_socks_pack(socks));
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
      return handle_pkt_reqrd(sockinfo, hi);
      
    case MSS_RSPWR:
      return handle_pkt_rspwr(sockinfo, hi);
      
    case MSS_REQFWD:
       return handle_pkt_reqfwd(sockinfo, hi);

    case MSS_RSPFWD:
       return handle_pkt_rspfwd(sockinfo, hi);

    case MSS_LSTN:
    case MSS_CLOSED:
    case MSS_NONE:
    default:
      abort();
    }
  }
  
  return 0;
}

enum handler_e handle_pkt_reqrd(struct middfs_sockinfo *sockinfo, const struct handler_info *hi) {
   return handle_pkt_rd(sockinfo, hi);
}
enum handler_e handle_pkt_rspfwd(struct middfs_sockinfo *sockinfo, const struct handler_info *hi) {
   return handle_pkt_rd(sockinfo, hi);
}

/* shared aux fn for reqrd and rspfwd */
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

  struct middfs_packet in_pkt;
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
  
  
  /* shutdown socket for incoming data */
#if SHUTDOWN
  if (shutdown(fd, SHUT_RD) < 0) {
    perror("shutdown");
    return HS_DEL;
  }
#endif

  /* remove used bytes */
  buffer_shift(buf_in, bytes_required);


  /* NOTE: We now need to determine whether this packet required forwarding or the server
   * can respond directly. If it required forwarding, then... */
  /* POSSIBLE STATES at this point: MSS_REQRD, MSS_RSPFWD.
   * Also, MSS_REQRD -> MSS_RSPWR | MSS_REQFWD, depending on the request.
   * But MSS_RSPFWD -> MSS_RSPWR. */
  
  /* Signal that data should now be exclusively written out. */
  /* TODO: dont' this t*/

  int tmpfd;
  switch (sockinfo->state) {
  case MSS_RSPFWD: /* just finished reading response */
     sockinfo->state = MSS_RSPWR;
     // tmpfd = sockinfo->in.fd;
     // sockinfo->in.fd = sockinfo->out.fd;
     // sockinfo->out.fd = tmpfd;
     if (buffer_serialize(&in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     break;

  case MSS_REQRD:
     sockinfo->state = MSS_REQFWD; /* exclusively forward for now. */
     if ((tmpfd = inet_connect(CLIENTB, LISTEN_PORT_DEFAULT)) < 0) {
        perror("inet_connect");
        return HS_DEL;
     }
     sockinfo->out.fd = tmpfd;
     if (buffer_serialize(&in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     break;

  default:
     abort();
  }

#if 0
  
  if (sockinfo->state == MSS_RSPWR) {
     
     
     //// TESTING /////
     struct buffer *buf_out = &sockinfo->out.buf;
     char *text = "this is the file you were looking for\n";
     
     sockinfo->state = MSS_RSPWR; /* new state: write response */
     sockinfo->out.fd = sockinfo->in.fd;
     sockinfo->in.fd = -1;


     
     if (buffer_copy(buf_out, text, strlen(text) + 1) < 0) {
        perror("buffer_copy");
        return HS_DEL;
     }
     ///// TESTING ////

  } else if (sockinfo->state == MSS_REQFWD) {
     if (buffer_serialize(&in_pkt, serialize_pkt, &buf_out) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     sockinfo->state = MSS_REQFWD;
     
     /* connect to client B */
     int clientb_fd;
     if ((clientb_fd = inet_connect(CLIENTB, LISTEN_PORT_DEFAULT)) < 0) {
        perror("inet_connect");
        return HS_DEL;
     }

     /* set this as new output */
     sockinfo->out.fd = clientb_fd;  
  }

#endif
      
  return HS_SUC;
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
  
  /* all of bytes written -- behavior depends on state */
#if SHUTDOWN  
  if (shutdown(fd, SHUT_WR) < 0) {
     perror("shutdown");
  }
#endif
  
  /* update state & return status */
  int tmpfd;
  switch (sockinfo->state) {
  case MSS_REQFWD:
     sockinfo->state = MSS_RSPFWD; /* now forward response */
     tmpfd = sockinfo->out.fd; /* invert polarity of socket */
     sockinfo->out.fd = sockinfo->in.fd;
     sockinfo->in.fd = tmpfd;
     return HS_SUC;
  case MSS_RSPWR:
     sockinfo->state = MSS_CLOSED; /* this socket will be closed */
     return HS_DEL;

  default:
     abort();
  }
}

enum handler_e handle_pkt_rspwr(struct middfs_sockinfo *sockinfo, const struct handler_info *hi) {
   return handle_pkt_wr(sockinfo, hi);
}
enum handler_e handle_pkt_reqfwd(struct middfs_sockinfo *sockinfo, const struct handler_info *hi) {
   return handle_pkt_wr(sockinfo, hi);
}

