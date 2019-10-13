/* middfs-server-conn.h -- middfs server connection manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERVER_CONN_H
#define __MIDDFS_SERVER_CONN_H

int server_start(const char *port, int backlog);
int server_accept(int servfd);

#endif
