/* middfs-server-conn.h -- middfs server connection manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERVER_CONN_H
#define __MIDDFS_SERVER_CONN_H

#include "middfs-sock.h"
#include "middfs-util.h"

#define LISTEN_PORT_DEFAULT 4321
#define LISTEN_PORT_DEFAULT_STR TOSTRING(LISTEN_PORT_DEFAULT)

int server_start(const char *port, int backlog);
int server_accept(int servfd);
int server_loop(struct middfs_socks *socks);

int handle_socket_event(nfds_t index, struct middfs_socks *socks);

int handle_req_event(nfds_t index, struct middfs_socks *socks);
int handle_connect_event(nfds_t index, struct middfs_socks *socks);
int handle_disconnect_event(nfds_t index, struct middfs_socks *socks);

int handle_req_outgoing(nfds_t index, struct middfs_socks *socks);
int handle_req_incoming(nfds_t index, struct middfs_socks *socks);


#endif

