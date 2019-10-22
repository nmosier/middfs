/* middfs-pkt.c -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <errno.h>

#include "middfs-pkt.h"

int packet_handle(struct middfs_packet *pkt) {
  const packet_handle_f handlers[MPKT_NTYPES] =
    {[MPKT_NONE] = packet_handle_none,
     [MPKT_CONNECT] = packet_handle_connect,
     [MPKT_DISCONNECT] = packet_handle_disconnect,
     [MPKT_REQUEST] = packet_handle_request,
    };
  int retv = 0;
						 
  /* validate packet */
  if (pkt->mpkt_magic != MPKT_MAGIC) {
    return -EINVAL; /* invalid magic number */
  }
  if (pkt->mpkt_type >= MPKT_NTYPES) {
    return -EINVAL; /* invalid packet type */
  }

  /* call packet handler */
  retv = handlers[pkt->mpkt_type](pkt);

  return retv;
}


int packet_handle_none(struct middfs_packet *pkt) {
  abort();
}

int packet_handle_connect(struct middfs_packet *pkt) {
  return -EOPNOTSUPP;
}

int packet_handle_disconnect(struct middfs_packet *pkt) {
  return -EOPNOTSUPP;
}

int packet_handle_request(struct middfs_packet *pkt) {
  struct middfs_request *req = &pkt->mpkt_un.mpkt_request;
  /* TODO: stub */

  return -EOPNOTSUPP;
}
