/* middfs-client-responder.h -- middfs client responder module.
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_CLIENT_RESPONDER_H
#define __MIDDFS_CLIENT_RESPONDER_H

struct client_responder_args {
  const char *port;
  int backlog;
};

void *client_responder(struct client_responder_args *args);

#endif
