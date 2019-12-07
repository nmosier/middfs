/* middfs-client-handler.h -- implementation of server-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_CLIENT_HANDLER_H
#define __MIDDFS_CLIENT_HANDLER_H

#include "lib/middfs-handler.h"

#define SERVER_IP "140.233.20.6"

extern struct handler_info client_hi;

/* prototype for request handlers that require opening the file */
typedef int (*handle_request_fd_f)(int fd, const struct middfs_request *req,
                                   struct middfs_response *rsp);

/* prototype for request handlers that only require the path */
typedef int (*handle_request_path_f)(const char *path, const struct middfs_request *req,
                                     struct middfs_response *rsp);

typedef union {
   handle_request_fd_f fd_f;
   handle_request_fd_f path_f;
} handle_request_f;


#endif
