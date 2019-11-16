/* middfs-server-handler.c -- implementation of server-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "server/middfs-server-handler.h"


#define CLIENTB "140.233.167.124" // nmosier's MacBook

static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
				    const struct middfs_packet *in_pkt) {
  int tmpfd;
  switch (sockinfo->state) {
  case MSS_RSPFWD: /* just finished reading response */
     sockinfo->state = MSS_RSPWR;
     if (buffer_serialize(in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     break;

  case MSS_REQRD:
     sockinfo->state = MSS_REQFWD; /* exclusively forward for now. */
     if ((tmpfd = inet_connect(CLIENTB, LISTEN_PORT_DEFAULT)) < 0) {
        perror("inet_connect");
        return HS_DEL;
     }
     sockinfo->out.fd = tmpfd;
     if (buffer_serialize(in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     break;

  default:
     abort();
  }

  return HS_SUC;
}

static enum handler_e handle_pkt_wr_fin(struct middfs_sockinfo *sockinfo) {
  /* update state & return status */
  int tmpfd;
  switch (sockinfo->state) {
  case MSS_REQFWD:
     sockinfo->state = MSS_RSPFWD; /* now forward response */
     tmpfd = sockinfo->out.fd; /* invert polarity of socket */
     sockinfo->out.fd = sockinfo->in.fd;
     sockinfo->in.fd = tmpfd;
     return HS_SUC;
  case MSS_RSPWR:
     sockinfo->state = MSS_CLOSED; /* this socket will be closed */
     return HS_DEL;

  default:
     abort();
  }
}

struct handler_info server_hi =
  {.rd_fin = handle_pkt_rd_fin,
   .wr_fin = handle_pkt_wr_fin
  };
