/* middfs-server-conn.h -- middfs server connection manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERVER_CONN_H
#define __MIDDFS_SERVER_CONN_H

#include "middfs-sock.h"
#include "middfs-util.h"
#include "middfs-handler.h"

#define SHOULD_RSPWR 1

#define LISTEN_PORT_DEFAULT 4321
#define LISTEN_PORT_DEFAULT_STR TOSTRING(LISTEN_PORT_DEFAULT)

int server_start(const char *port, int backlog, struct middfs_socks *socks);
int server_accept(int servfd);
int server_loop(struct middfs_socks *socks, const struct handler_info *hi);

enum handler_e handle_socket_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				   struct middfs_sockinfo *new_sockinfo);
enum handler_e handle_lstn_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi,
				 struct middfs_sockinfo *new_sockinfo);
enum handler_e handle_pkt_event(struct middfs_sockinfo *sockinfo, const struct handler_info *hi);

#endif

