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

  /* iterate through sockets and handle events */
  for (int i = 0; i < socks->count; ++i) {
    struct middfs_sockinfo *info = &socks->sockinfos[i];
    int revents = info->revents;

    /* remove socket if error occured */
    if (revents & (POLLERR | POLLHUP)) {
      if (middfs_socks_remove(i, socks) < 0) {
	return -1;
      }
      continue;
    }

    /* TODO -- handle socket event */
  }


  
  for (int index = 0; index < socks->count; ++index) {

    
    int revents = socks->pollfds[index].revents;

    /* remove socket if error occurred */
    if (revents & (POLLERR | POLLHUP)) {
      if (middfs_socks_remove(index, socks) < 0) {
	return -1;
      }
      --index;
      continue;
    }

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
  switch (sockinfo->type) {
  case MFD_PKT_IN:
    return handle_pkt_event(sockinfo, hi, new_sockinfo);

  case MFD_PKT_OUT:
    abort(); /* TODO */
        
  case MFD_LSTN: /* POLLIN -- accept new client connection */
    return handle_lstn_event(sockinfo, hi, new_sockinfo);
    
  case MFD_NONE:
  default:
    abort();
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

enum handler_e handle_pkt_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				struct middfs_sockinfo *new_sockinfo) {
  int revents = sockinfo->revents;

  if (revents & POLLIN) {
    return handle_pkt_incoming(sockinfo, hi, new_sockinfo);
  }
  if (revents & POLLOUT) {
    return handle_pkt_outgoing(sockinfo, hi, new_sockinfo);
  }

  return 0;
}

/* Called when POLLIN set -- function name should indicate this 
 * TODO */
enum handler_e handle_pkt_incoming(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				   struct middfs_sockinfo *new_sockinfo) {
  struct middfs_sockend *in = &sockinfo->in;
  int fd = in->fd;
  struct buffer *buf_in = &in->buf;
  ssize_t bytes_read;

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
  
  /* remove used bytes */
  buffer_shift(buf_in, bytes_required);
  
  /* shutdown socket for incoming data */
  if (shutdown(fd, SHUT_RD) < 0) {
    perror("shutdown");
    return HS_DEL;
  }

  /* TODO: This logic shoud be improved. Add transition function. */

  /* Signal that data should now be exclusively written out. */
  sockinfo->out.fd = sockinfo->in.fd;
  sockinfo->in.fd = -1; 

  //// TESTING /////
  /* TODO: This is where the handler needs to be called. */
  struct buffer *buf_out = &sockinfo->out.buf;
  char *text = "this is the file you were looking for\n";

  if (buffer_copy(buf_out, text, strlen(text) + 1) < 0) {
    perror("buffer_copy");
    return HS_DEL;
  }
  ///// TESTING ////
      
  return HS_SUC;
}

enum handler_e handle_pkt_outgoing(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
			struct middfs_sockinfo *new_sockinfo) {
  int fd = sockinfo->out.fd;
  struct buffer *buf_out = &sockinfo->out.buf;

  ssize_t bytes_written = buffer_write(fd, buf_out);
  if (bytes_written < 0) {
    /* error */
    perror("buffer_write");
    return HS_DEL;
  }
  if (bytes_written == 0) {
    /* all of bytes written */
    if (shutdown(fd, SHUT_WR) < 0) {
      perror("shutdown");
    }
    return HS_DEL;
  }
  
  return HS_SUC;
}
