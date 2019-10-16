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

#include "middfs-server-conn.h"

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

  int readyfds = poll(socks->pollfds, socks->count, INFTIM);
  if (readyfds < 0) {
    perror("server_loop: poll");
    return -1;
  }

  for (int index = 0; index < socks->count; ++index) {
    int revents = socks->pollfds[index].revents;
    int fd = socks->pollfds[index].fd;

    /* remove socket if error occurred */
    if (revents & (POLLERR | POLLHUP)) {
      if (middfs_socks_remove(index, socks) < 0) {
	abort(); /* TODO */
      }
      --index;
      continue;
    }

    /* */
    int client_sockfd; /* for MFD_LSTN */
    switch (socks->sockinfos[index].type) {
    case MFD_CREQ:
      if (revents & POLLIN) {
	// TEST //
	int bytes_read;
	char buf[64];
	bytes_read = read(fd, buf, 64);
	printf("%s", buf);
	// TEST //
      }
      break;
      
    case MFD_SREQ:
      /* TODO -- not yet implemented */
      abort();
      
    case MFD_LSTN: /* POLLIN -- accept new client connection */
      if (revents == POLLIN) {
	if ((client_sockfd = server_accept(fd)) >= 0) {
	  /* valid client connection, so add to socket list */
	  struct middfs_sockinfo sockinfo = {MFD_CREQ};
	  if (middfs_socks_add(client_sockfd, &sockinfo, socks) < 0) {
	    perror("middfs_socks_add"); /* TODO -- resize should perr */
	    close(client_sockfd);
	    continue;
	  }
	}
      }
      break;
      
    case MFD_NONE:
    default:
      abort();
    }
  }
  
  
  return retv;
  /* TODO: perhaps this should return the number of open 
   * sockets -- if this ever reaches 0, then the server should shut
   * down. */
}


#if 0
/* server_loop()
 * DESC: repeatedly poll(2)'s server socket for new connections to accept and client sockets
 *       for (i) more request data to receive and then (ii) more response data to send. Returns
 *       once server_accepting is 0 and all requests have been serviced.
 * ARGS:
 *  - servfd: server socket file descriptor.
 *  - ftypes: pointer to content type table.
 * RETV: 0 upon success, -1 upon error.
 * NOTE: prints errors.
 */
int server_loop(int servfd) {
  middfs_socks socks;
   int retv = 0;
   int shutdwn = 0;

   /* intialize variables */
   retv = 0;
   shutdwn = 0;
   httpfds_init(&hfds);
   
   /* insert server socket to list */
   if (httpfds_insert(servfd, POLLIN, &hfds) < 0) {
      perror("httpfds_insert");
      if (httpfds_delete(&hfds) < 0) {
         perror("httpfds_delete");
      }
      return -1;
   }

   /* service clients as long as sockets open & fatal error hasn't occurred */
   while (retv >= 0 && (server_accepting || hfds.nopen > 1)) {
      int nready;

      /* if no longer accepting, stop reading */
      if (!server_accepting && !shutdwn) {
         if (shutdown(servfd, SHUT_RD) < 0) {
            perror("shutdown");
            if (httpfds_delete(&hfds) < 0) {
               perror("httpfds_delete");
            }
            return -1;
         }
         shutdwn = 1;
      }
      
      /* poll for new connections / reading requests / sending responses */
      if ((nready = poll(hfds.fds, hfds.count, -1)) < 0) {
         if (errno != EINTR) {
            perror("poll");
            if (httpfds_delete(&hfds) < 0) {
               perror("httpfds_delete");
            }
            return -1;
         }
         continue;
      }
   
      if (DEBUG) {
         fprintf(stderr, "poll: %d descriptors ready\n", nready);
      }
      
      for (size_t i = 0; nready > 0; ++i) {
         int fd;
         int revents;
         
         fd = hfds.fds[i].fd;
         revents = hfds.fds[i].revents;
         if (fd >= 0 && revents) {
            if (fd == servfd) {
               if (handle_pollevents_server(fd, revents, &hfds) < 0) {
                  fprintf(stderr, "server_loop: server socket error\n");
                  if (httpfds_delete(&hfds) < 0) {
                     perror("httpfds_delete");
                  }
                  return -1;
               }
            } else {
               if (handle_pollevents_client(fd, i, revents, &hfds, ftypes) < 0) {
                  return -1;
               }
            }
            
            --nready;
         }
         
      }

      /* pack httpfds in case some connections were closed */
      httpfds_pack(&hfds);
   }

   /* remove (& close) all client sockets */
   hfds.fds[0].fd = -1; // don't want to close server socket
   if (httpfds_delete(&hfds) < 0) {
      perror("httpfds_delete");
      retv = -1;
   }

   return retv;
}

#endif
