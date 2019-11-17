/* middfs-client-responder.c -- middfs client responder module.
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>

#include "lib/middfs-sock.h"
#include "lib/middfs-conn.h"

#include "client/middfs-client-responder.h"
#include "client/middfs-client-handler.h"

void *client_responder(struct client_responder_args *args) {
  /* start the server */
  struct middfs_socks socks;
  middfs_socks_init(&socks);
  if (server_start(args->port, args->backlog, &socks) < 0) {
    perror("client_responder: server_start");
    goto cleanup;
  }

  while (server_loop(&socks, &client_hi) >= 0) {}
  
 cleanup:
  free(args);
  
  return NULL;
}
