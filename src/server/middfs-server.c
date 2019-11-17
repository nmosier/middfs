/* middfs-server.c --  middfs server main() routine
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "lib/middfs-sock.h"
#include "lib/middfs-conn.h"
#include "lib/middfs-rsrc.h"
#include "lib/middfs-util.h"

#include "server/middfs-server-handler.h"
#include "server/middfs-client.h"
#include "server/middfs-server.h"

struct clients clients; /* list of connected clients */

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
  struct middfs_socks socks;
  middfs_socks_init(&socks);
  clients_init(&clients);

  if (server_start(listen_port, 10, &socks) < 0) {
    return 2;
  }

  /* call server loop */
  while (server_loop(&socks, &server_hi) >= 0) {}

  /* cleanup */
  if (middfs_socks_delete(&socks) < 0) {
    exitno = 5;
  }

  return exitno;
}

