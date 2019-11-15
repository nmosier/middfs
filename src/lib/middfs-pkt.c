/* middfs-pkt.c -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "middfs-pkt.h"

bool req_has_mode(enum middfs_request_type type) {
  return type == MREQ_ACCESS || type == MREQ_CHMOD || type == MREQ_CREATE || type == MREQ_OPEN;
}

bool req_has_size(enum middfs_request_type type) {
  return type == MREQ_READLINK || type == MREQ_TRUNCATE || type == MREQ_READ || type == MREQ_WRITE;
}

bool req_has_to(enum middfs_request_type type) {
  return type == MREQ_SYMLINK || type == MREQ_RENAME;
}

bool req_has_off(enum middfs_request_type type) {
  return type == MREQ_READ || type == MREQ_WRITE;
}

#if 0
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
  /* TODO: stub */

  return -EOPNOTSUPP;
}
#endif
