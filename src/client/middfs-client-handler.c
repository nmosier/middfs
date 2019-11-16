/* middfs-client-handler.c -- implementation of client-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "client/middfs-client-handler.h"

static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
				 const struct middfs_packet *in_pkt) {
  assert(sockinfo->state == MSS_REQRD);
  
  /* Formulate response packet */
  char *placeholder_string = "Greetings from a client of middfs.\n";

  struct middfs_response rsp =
    {.nbytes = strlen(placeholder_string) + 1,
     .data = placeholder_string
    };
  struct middfs_packet out_pkt =
    {.mpkt_magic = MPKT_MAGIC,
     .mpkt_type = MPKT_RESPONSE,
     .mpkt_un = {.mpkt_response = rsp}
    };

  /* Serialize response packet */
  if (buffer_serialize(&out_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
    perror("buffer_serialize");
    return HS_DEL;
  }

  /* Update state of socket for writing response */
  sockinfo->state = MSS_RSPWR;
  sockinfo->out.fd = sockinfo->in.fd;

  return HS_SUC; /* keep socket open */
}

static enum handler_e handle_pkt_wr_fin(struct middfs_sockinfo *sockinfo) {
  /* Nothing to do except delete socket from list. */
  assert(sockinfo->state == MSS_RSPWR);
  return HS_DEL;
}

struct handler_info client_hi =
  {.rd_fin = handle_pkt_rd_fin,
   .wr_fin = handle_pkt_wr_fin
  };
