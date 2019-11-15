/* middfs-handle.h -- middfs  handler module 
 * Nicholas Mosier & Tommaso Monaco 2019 
 */

#ifndef __MIDDFS_HANDLE_H
#define __MIDDFS_HANDLE_H

#include <stdio.h>

#include "lib/middfs-serial.h"
#include "lib/middfs-sock.h"

/* Handler function return value enum */
enum handler_e {
   HS_ERR = -1, /* An internal error occurred. */
   HS_SUC,      /* Nothing special. */
   HS_NEW,      /* Create new socket.  */
   HS_DEL,      /* An error has occurred during handling of packet; socket should be deleted. */
};


struct handler_info;

typedef enum handler_e (*pkt_handler_f)(struct middfs_sockinfo *sockinfo,
				      const struct handler_info *hi);


/* Handler Info */
struct handler_info {
  pkt_handler_f reqrd;
  pkt_handler_f reqfwd;
  pkt_handler_f rspfwd;
  pkt_handler_f rspwr;
};

#endif
