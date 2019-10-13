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
