/* middfs-server.c --  middfs server main() routine
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "middfs-sock.h"
#include "middfs-conn.h"
#include "middfs-rsrc.h"
#include "middfs-util.h"

static int server_handle_pkt(const struct middfs_packet *in, struct middfs_packet *out);

int main(int argc, char *argv[]) {
  int exitno = 0;
  char *listen_port = LISTEN_PORT_DEFAULT_STR;
  
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
  if ((servfd = server_start(listen_port, 10)) < 0) {
    return 2;
  }

  /* initialize socket list to poll on */
  struct middfs_socks socks;
  middfs_socks_init(&socks);

  /* add server listening socket to socket list to poll */
  //  struct middfs_sockinfo serv_sockinfo = {MFD_LSTN};
  struct middfs_sockinfo serv_sockinfo;
  middfs_sockinfo_init(MFD_LSTN, servfd, -1, &serv_sockinfo);
  if (middfs_socks_add(&serv_sockinfo, &socks) < 0) {
    exitno = 4;
    close(servfd);
    goto cleanup;
  }

  /* call server loop */
  struct handler_info hi =
     {.handle_in = server_handle_pkt};
  
  while (server_loop(&socks, &hi) >= 0) {}
  
 cleanup:
  if (middfs_socks_delete(&socks) < 0) {
    exitno = 5;
  }

  return exitno;
}


static int server_handle_pkt(const struct middfs_packet *in, struct middfs_packet *out) {
  *out = *in;
  return 0;
}
