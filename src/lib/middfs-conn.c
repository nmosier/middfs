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


int server_loop(struct middfs_socks *socks) {
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
	abort(); /* TODO */
      }
      --index;
      continue;
    }

    int status = handle_socket_event(index, socks);
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

int handle_socket_event(nfds_t index, struct middfs_socks *socks) {
  int revents = socks->pollfds[index].revents;
  int fd = socks->pollfds[index].fd;
  struct middfs_sockinfo *sockinfo = &socks->sockinfos[index];

  int client_sockfd; /* for MFD_LSTN */
    switch (sockinfo->type) {
    case MFD_CREQ:
      if (revents & POLLIN) {
	struct buffer *buf_in = &sockinfo->buf_in;
	
	/* prepare for writing */
	if (buffer_increase(buf_in) < 0) {
	  perror("buffer_increase");
	  abort(); /* TODO */
	};
	
	size_t rem = buffer_rem(buf_in);
	ssize_t bytes_read = 0;

	if ((bytes_read = read(fd, buf_in->ptr, rem))  < 0) {
	  perror("read");
	  abort(); /* TODO */
	} else if (bytes_read == 0) {
	  /* data ended prematurely -- remove socket from list */
	  return 1;
	} else {
	  /* update buffer pointers */
	  buffer_advance(buf_in, bytes_read);

	  /* try to deserialize buffer */
	  /* TODO: this should be handled by packet handler. */
	  struct rsrc rsrc;
	  int errp = 0;
	  size_t bytes_ready = buffer_used(buf_in);
	  size_t bytes_required = deserialize_rsrc(buf_in->begin,
						   bytes_ready, &rsrc,
						   &errp);
	  
	  if (errp) {
	    /* invalid data; close socket */
	    return 1;
	    
	  } else {
	    if (bytes_required <= bytes_ready) {
	      /* successfully deserialized packet */
	      print_rsrc(&rsrc);
	      
	      
	      /* remove used bytes */
	      buffer_shift(buf_in, bytes_required);

	      /* TODO: leave socket open for response. */
	      return 1;
	      
	    } else {
	      /* read more bytes */
	      /* TODO: Make sure pollfd event POLLIN is set */
	    }
	  }
	}
      }

      break;
      
    case MFD_SREQ:
      /* TODO -- not yet implemented */
      abort();
      
    case MFD_LSTN: /* POLLIN -- accept new client connection */
      if (revents | POLLIN) {
	if ((client_sockfd = server_accept(fd)) >= 0) {
	  /* valid client connection, so add to socket list */
	  struct middfs_sockinfo sockinfo = {MFD_CREQ};
	  if (middfs_socks_add(client_sockfd, &sockinfo, socks) < 0) {
	    perror("middfs_socks_add"); /* TODO -- resize should perr */
	    close(client_sockfd);
	    abort(); /* TODO -- graceful exit. */
	  }
	}
      }
      break;
      
    case MFD_NONE:
    default:
      abort();
    }

 return 0;
}
