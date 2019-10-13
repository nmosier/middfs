/* middfs-server.c --  middfs server main() routine
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <limits.h>

#include "middfs-server-conn.h"

#define LISTEN_PORT_DEFAULT 4321

int main(int argc, char *argv[]) {
  int listen_port = LISTEN_PORT_DEFAULT;
  
  /* parse command-line args */
  int c;
  char *optstring = "p:h";
  const char *usage = "usage: %s [-p <listen-port>] <mountpoint>\n";
  char *endptr;
  unsigned long long_tmp;
  int optvalid = 1;
  while ((c = getopt(argc, argv, optstring)) >= 0) {
    switch (c) {
    case 'p':
      long_tmp = strtoul(optarg, &endptr, 0);
      if (*endptr != '\0' || long_tmp > INT_MAX) {
	fprintf(stderr, "%s: -p: invalid listening port %s\n", argv[0],
		optarg);
	optvalid = 0;
      }
      listen_port = (int) long_tmp;
      break;
      
    case 'h':
    case '?':
    default:
      fprintf(stderr, usage, argv[0]);
      optvalid = 0;
    }
  }

  if (!optvalid) {
    exit(1);
  }

  if (argc - optind != 1) {
    fprintf(stderr, usage, argv[0]);
    exit(1);
  }



  //// TESTING /////
  int servfd = -1;
  if ((servfd = server_start("4321", 10)) < 0) {
    exit(2);
  }

  int clientfd = -1;
  if ((clientfd = server_accept(servfd)) < 0) {
    exit(3);
  }

  printf("client fd = %d\n", clientfd);

  return 0;
}
