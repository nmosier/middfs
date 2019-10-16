/* middfs-server.c --  middfs server main() routine
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "middfs-sock.h"
#include "middfs-server-conn.h"
#include "middfs-rsrc.h"
#include "middfs-util.h"

#define LISTEN_PORT_DEFAULT "4321"

int main(int argc, char *argv[]) {
  char *listen_port = LISTEN_PORT_DEFAULT;
  int exitno = 0;
  
  /* parse command-line args */
  int c;
  char *optstring = "p:h";
  const char *usage = "usage: %s [-p <listen-port>] <mountpoint>\n";
  int optvalid = 1;
  while ((c = getopt(argc, argv, optstring)) >= 0) {
    switch (c) {
    case 'p':
      listen_port = optarg;
      break;
    case 'h':
    case '?':
    default:
      fprintf(stderr, usage, argv[0]);
      optvalid = 0;
    }
  }

  if (!optvalid) {
    return 1;
  }

  if (argc - optind != 1) {
    fprintf(stderr, usage, argv[0]);
    return 1;
  }

  /* start server for listening */
  int servfd; /* server file descriptor */
  if ((servfd = server_start("4321", 10)) < 0) {
    return 2;
  }

  /* initialize socket list to poll on */
  struct middfs_socks socks;
  if (middfs_socks_init(&socks) < 0) {
    exitno = 3;
    close(servfd);
    return 3;
  }

  /* add server listening socket to socket list to poll */
  struct middfs_sockinfo serv_sockinfo = {MFD_LSTN};
  if (middfs_socks_add(servfd, &serv_sockinfo, &socks) < 0) {
    exitno = 4;
    close(servfd);
    goto cleanup;
  }

  /* call server loop */
  while (server_loop(&socks) >= 0) {}

#if 0

  int clientfd = -1;
  if ((clientfd = server_accept(servfd)) < 0) {
    return 3;
  }

  
  printf("client fd = %d\n", clientfd);

  char rsrc_buff[64*sizeof(char)];
  struct rsrc rsrc;
  int errp = 0;
  ssize_t nbytes;

  /* read bytes into buffer */
  if ((nbytes = read(clientfd, rsrc_buff, 64)) < 0) {
    perror("read");
  }
  
  /* test resource */
  deserialize_rsrc(rsrc_buff, (size_t) nbytes, &rsrc, &errp);

  printf("client's name is %s\n", rsrc.mr_owner);
  printf("client's file path %s\n", rsrc.mr_path);
#endif  
  
 cleanup:
  if (middfs_socks_delete(&socks) < 0) {
    exitno = 5;
  }

  return exitno;
}
