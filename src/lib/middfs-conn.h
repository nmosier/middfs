/* middfs-server-conn.h -- middfs server connection manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERVER_CONN_H
#define __MIDDFS_SERVER_CONN_H

#include "middfs-sock.h"

int server_start(const char *port, int backlog);
int server_accept(int servfd);
int server_loop(struct middfs_socks *socks);

int handle_socket_event(nfds_t index, struct middfs_socks *socks);

#endif

