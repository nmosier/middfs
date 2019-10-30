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

  int readyfds = poll(socks->pollfds, socks->count, -1);
  if (readyfds < 0) {
    perror("server_loop: poll");
    return -1;
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

    int status = handle_socket_event(index, socks, hi);
    if (status > 0) {
      /* remove socket */
      if (middfs_socks_remove(index, socks) < 0) {
	return -1;
      }
    } else if (status < 0) {
      return -1;
    }
    
    fprintf(stderr, "nopen=%d\n", middfs_socks_pack(socks));
  }
  
  return retv;
  /* TODO: perhaps this should return the number of open 
   * sockets -- if this ever reaches 0, then the server should shut
   * down. */
}

int handle_socket_event(nfds_t index, struct middfs_socks *socks,
			const struct handler_info *hi) {
  struct middfs_sockinfo *sockinfo = &socks->sockinfos[index];

  switch (sockinfo->type) {
  case MFD_PKT:
    return handle_pkt_event(index, socks, hi);
        
  case MFD_LSTN: /* POLLIN -- accept new client connection */
    return handle_lstn_event(index, socks);
    
  case MFD_NONE:
  default:
    abort();
  }

  return 0;
}

int handle_lstn_event(nfds_t index, struct middfs_socks *socks) {
  int revents = socks->pollfds[index].revents;
  int fd = socks->pollfds[index].fd;
  int client_sockfd;
  
  if (revents & POLLIN) {
    if ((client_sockfd = server_accept(fd)) >= 0) {
      /* valid client connection, so add to socket list */
      struct middfs_sockinfo sockinfo = {MFD_PKT};
      if (middfs_socks_add(client_sockfd, &sockinfo, socks) < 0) {
	perror("middfs_socks_add"); /* TODO -- resize should perr */
	close(client_sockfd);
	return -1;
      }
    }
  }

  return 0;
}

int handle_pkt_event(nfds_t index, struct middfs_socks *socks, const struct handler_info *hi) {
  struct pollfd *pollfd = &socks->pollfds[index];
  int revents = pollfd->revents;

  if (revents & POLLIN) {
    return handle_pkt_incoming(index, socks, hi);
  }
  if (revents & POLLOUT) {
    return handle_pkt_outgoing(index, socks, hi);
  }

  return 0;
}

int handle_pkt_incoming(nfds_t index, struct middfs_socks *socks, const struct handler_info *hi) {
  struct pollfd *pollfd = &socks->pollfds[index];
  int fd = pollfd->fd;
  struct middfs_sockinfo *sockinfo = &socks->sockinfos[index];

  struct buffer *buf_in = &sockinfo->buf_in;
  ssize_t bytes_read = buffer_read(fd, buf_in);

  if (bytes_read < 0) {
    perror("buffer_read");
    middfs_socks_remove(index, socks);
    return -1;
  }
  if (bytes_read == 0) {
    /* data ended prematurely -- remove socket from list */
    return middfs_socks_remove(index, socks);
  }

  struct middfs_packet pkt;
  int errp = 0;
  size_t bytes_ready = buffer_used(buf_in);

  size_t bytes_required = deserialize_pkt(buf_in->begin, bytes_ready, &pkt, &errp);
  
  if (errp) {
    /* invalid data; close socket */
    return middfs_socks_remove(index, socks);
  }

  if (bytes_required > bytes_ready) {
    return 0;
  }

  /* remove used bytes */
  buffer_shift(buf_in, bytes_required);
  
  /* shutdown socket for incoming data */
  if (shutdown(fd, SHUT_RD) < 0) {
    perror("shutdown");
    middfs_socks_remove(index, socks);
    return -1;
  }

  //// TESTING /////
  /* TODO: This is where the handler needs to be called. */
  struct buffer *buf_out = &sockinfo->buf_out;
  char *text = "this is the file you were looking for\n";

  if (buffer_copy(buf_out, text, strlen(text) + 1) < 0) {
    perror("buffer_copy");
    middfs_socks_remove(index, socks);
    return -1;
  }
  ///// TESTING ////

    
  /* configure socket for outgoing data */
  socks->pollfds[index].events = POLLOUT;      
  
  return 0;
}

int handle_pkt_outgoing(nfds_t index, struct middfs_socks *socks, const struct handler_info *hi) {
  /* TODO: implement correctly. This is just a test. */
  int fd = socks->pollfds[index].fd;
  struct middfs_sockinfo *sockinfo = &socks->sockinfos[index];
  struct buffer *buf_out = &sockinfo->buf_out;

  ssize_t bytes_written = buffer_write(fd, buf_out);

  if (bytes_written < 0) {
    /* error */
    perror("buffer_write");
    middfs_socks_remove(index, socks);
    return -1;
  }
  if (bytes_written == 0) {
    /* all of bytes written */
    middfs_socks_remove(index, socks);
  }
  
  return 0;
}
