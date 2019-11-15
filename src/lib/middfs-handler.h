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

typedef enum handler_e (*handle_pkt_rd_f)(struct middfs_sockinfo *sockinfo,
					  const struct middfs_packet *in_pkt);
typedef enum handler_e (*handle_pkt_wr_f)(struct middfs_sockinfo *sockinfo);

/* Handler Info */
struct handler_info {
  /* These functions are called when a packet has been fully written/received.
   * They shall update the socket state (MSS_*) and internal fds of the socket.
   */
  handle_pkt_rd_f rd_fin; /* handle packet that has been fully read/received */
  handle_pkt_wr_f wr_fin; /* handle when a packet has been fully written/sent */
};

#endif
