/* middfs-server-handler.c -- implementation of server-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "server/middfs-server-handler.h"


#define CLIENTB "140.233.167.124" // nmosier's MacBook

static enum handler_e handle_pkt_wr(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);
static enum handler_e handle_pkt_rd(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi);


static enum handler_e handle_pkt_rspwr(struct middfs_sockinfo *sockinfo,
				       const struct handler_info *hi) {
   return handle_pkt_wr(sockinfo, hi);
}
static enum handler_e handle_pkt_reqfwd(struct middfs_sockinfo *sockinfo,
					const struct handler_info *hi) {
   return handle_pkt_wr(sockinfo, hi);
}

static enum handler_e handle_pkt_reqrd(struct middfs_sockinfo *sockinfo,
				       const struct handler_info *hi) {
   return handle_pkt_rd(sockinfo, hi);
}
static enum handler_e handle_pkt_rspfwd(struct middfs_sockinfo *sockinfo,
					const struct handler_info *hi) {
   return handle_pkt_rd(sockinfo, hi);
}

/* shared aux fn for reqrd and rspfwd */
static enum handler_e handle_pkt_rd(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi) {
  struct middfs_sockend *in = &sockinfo->in;
  int fd = in->fd;
  struct buffer *buf_in = &in->buf;
  ssize_t bytes_read;

  assert(sockinfo->revents & POLLIN);

  /* read bytes into buffer */
  bytes_read = buffer_read(fd, buf_in);
  if (bytes_read < 0) {
    perror("buffer_read");
    return HS_DEL; /* delete socket */
  }
  if (bytes_read == 0) {
    /* data ended prematurely -- remove socket from list */
    fprintf(stderr, "warning: data ended prematurely for socket %d\n", fd);
    return HS_DEL;
  }

  struct middfs_packet in_pkt;
  int errp = 0;
  size_t bytes_ready = buffer_used(buf_in);
  size_t bytes_required = deserialize_pkt(buf_in->begin, bytes_ready, &in_pkt, &errp);
  
  if (errp) {
    /* invalid data; close socket */
    fprintf(stderr, "warning: packet from socket %d contains invalid data\n", fd);
    return HS_DEL;
  }

  if (bytes_required > bytes_ready) {
    return HS_SUC; /* wait on more bytes */
  }

  /* incoming packet has been successfully deserialized */
  
  
  /* shutdown socket for incoming data */
#if SHUTDOWN
  if (shutdown(fd, SHUT_RD) < 0) {
    perror("shutdown");
    return HS_DEL;
  }
#endif

  /* remove used bytes */
  buffer_shift(buf_in, bytes_required);


  /* NOTE: We now need to determine whether this packet required forwarding or the server
   * can respond directly. If it required forwarding, then... */
  /* POSSIBLE STATES at this point: MSS_REQRD, MSS_RSPFWD.
   * Also, MSS_REQRD -> MSS_RSPWR | MSS_REQFWD, depending on the request.
   * But MSS_RSPFWD -> MSS_RSPWR. */
  
  /* Signal that data should now be exclusively written out. */
  /* TODO: dont' this t*/

  int tmpfd;
  switch (sockinfo->state) {
  case MSS_RSPFWD: /* just finished reading response */
     sockinfo->state = MSS_RSPWR;
     if (buffer_serialize(&in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
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
     if (buffer_serialize(&in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
        perror("buffer_serialize");
        return HS_DEL;
     }
     break;

  default:
     abort();
  }

  return HS_SUC;
}


/* shared aux fn for rspwr and reqfwd */
static enum handler_e handle_pkt_wr(struct middfs_sockinfo *sockinfo,
                                    const struct handler_info *hi) {
  int fd = sockinfo->out.fd;
  struct buffer *buf_out = &sockinfo->out.buf;

  assert(sockinfo->revents & POLLOUT);
  
  ssize_t bytes_written = buffer_write(fd, buf_out);
  if (bytes_written < 0) {
    /* error */
    perror("buffer_write");
    return HS_DEL;
  }

  /* successful write, but more bytes to write, so don't change state */
  if (bytes_written > 0) {
     return HS_SUC;
  }
  
  /* all of bytes written -- behavior depends on state */
#if SHUTDOWN  
  if (shutdown(fd, SHUT_WR) < 0) {
     perror("shutdown");
  }
#endif
  
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
  {.reqrd = handle_pkt_reqrd,
   .reqfwd = handle_pkt_reqfwd,
   .rspfwd = handle_pkt_rspfwd,
   .rspwr = handle_pkt_rspwr
  };
