/* middfs-util.c -- utility functions
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "middfs-util.h"

/* sizerem() -- get remaining size given total size and used size.
 * RETV: if _nbytes_ >= _used_, returns _nbytes_ - _used_;
 *       otherwise returns 0.
 */
size_t sizerem(size_t nbytes, size_t used) {
  return nbytes >= used ? nbytes - used : 0;
}

size_t smin(size_t s1, size_t s2) {
  return (s1 < s2) ? s1 : s2;
}

size_t smax(size_t s1, size_t s2) {
  return (s1 < s2) ? s2 : s1;
}

/* inet_connect() -- connect to server at given IP
 *                   on given port.
 * ARGS:
 *  - IP_addr: IPv6 address of server, as a string
 *  - port: port to connect on
 * RETV: see connect(2)
 */
int inet_connect(const char *IP_addr, int port) {
  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    return -1; /* socket(2) error */
  }
  struct sockaddr_in addr = {0};
  addr.sin_addr.s_addr = inet_addr(IP_addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
    return -1; /* connect(2) error */
  }

  return sockfd; /* success */
}

