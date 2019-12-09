/* middfs-pkt.c -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "lib/middfs-pkt.h"
#include "lib/middfs-buf.h"
#include "lib/middfs-conf.h"

#include "client/middfs-client-conf.h"

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

bool req_has_data(enum middfs_request_type type) {
   return type == MREQ_WRITE;
}

void packet_error(struct middfs_packet *pkt, int error) {
   pkt->mpkt_magic = MPKT_MAGIC;
   pkt->mpkt_type = MPKT_RESPONSE;

   struct middfs_response *rsp = &pkt->mpkt_un.mpkt_response;
   rsp->mrsp_type = MRSP_ERROR;
   rsp->mrsp_un.mrsp_error = error;
}


/* PACKET SUBTYPE INITIALIZATION FUNCTIONS */

void request_init(struct middfs_request *req, enum middfs_request_type type,
                  const struct rsrc *rsrc) {
   req->mreq_type = type;
   req->mreq_requester = conf_get(MIDDFS_CONF_USERNAME);
   req->mreq_rsrc = *rsrc;
}

void response_init(struct middfs_response *rsp, enum middfs_response_type type) {
   rsp->mrsp_type = type;
}

int connect_init(struct middfs_connect *conn) {
   int err = 0;
   
   conn->name = conf_get(MIDDFS_CONF_USERNAME);
   conn->port = conf_get_uint32(MIDDFS_CONF_LOCALPORT, &err);

   if (err) {
      return -1;
   }
   return 0;
}

/* TODO: Disconnect. */

/* PACKET INITIALIZATION FUNCTIONS */

/* packet_init() -- initialize bare packet */
void packet_init(struct middfs_packet *pkt, enum middfs_packet_type type) {
   pkt->mpkt_magic = MPKT_MAGIC;
}
