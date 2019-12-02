/* middfs-client-handler.c -- implementation of client-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "client/middfs-client-handler.h"
#include "client/middfs-client-rsrc.h"

static enum handler_e handle_request(const struct middfs_packet *in_pkt,
                                     struct middfs_packet *out_pkt);


static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
				 const struct middfs_packet *in_pkt) {
   int retv;
   
  assert(sockinfo->state == MSS_REQRD);

#if 0
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
#else
  struct middfs_packet out_pkt;

  switch (in_pkt->mpkt_type) {
  case MPKT_NONE:
     fprintf(stderr, "handle_pkt_rd_fin: packet is of type NONE\n");
     return HS_DEL;
     
  case MPKT_CONNECT:
  case MPKT_DISCONNECT:
  case MPKT_RESPONSE:
     fprintf(stderr, "handle_pkt_rd_fin: packet type not implemented yet\n");
     return HS_DEL;

  case MPKT_REQUEST:
     retv = handle_request(in_pkt, &out_pkt);
     break;
     
  default:
     fprintf(stderr, "handle_pkt_rd_fin: unrecognized packet type %d\n", in_pkt->mpkt_type);
     return HS_DEL;
  }

  if (retv == HS_DEL) {
     return HS_DEL;
  }

#endif
  
  /* serialize response packet */
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



static enum handler_e handle_request_read(const struct middfs_request *req,
                                     struct middfs_response *rsp);

/* handle_request() -- handle a request and queue a response (if necessary).
 */
static enum handler_e handle_request(const struct middfs_packet *in_pkt,
                                     struct middfs_packet *out_pkt) {
   const struct middfs_request *req = &in_pkt->mpkt_un.mpkt_request;
   struct middfs_response *rsp = &out_pkt->mpkt_un.mpkt_response;

   /* response packet setup */
   out_pkt->mpkt_magic = MPKT_MAGIC;
   out_pkt->mpkt_type = MPKT_RESPONSE;
   
   switch (req->mreq_type) {
   case MREQ_NONE:
      /* Then why the hell did you contact me? */
      fprintf(stderr, "handle_request: request of type ``MREQ_NONE''\n");
      return HS_DEL;

   case MREQ_GETATTR:
   case MREQ_ACCESS:
   case MREQ_READLINK:
   case MREQ_MKDIR:
   case MREQ_SYMLINK:
   case MREQ_UNLINK:
   case MREQ_RMDIR:
   case MREQ_RENAME:
   case MREQ_CHMOD:
   case MREQ_TRUNCATE:
   case MREQ_OPEN:
   case MREQ_CREATE:
   case MREQ_WRITE:
   case MREQ_READDIR:
      fprintf(stderr, "handle_request: request not implemented yet\n");
      return HS_DEL;

   case MREQ_READ:
      return handle_request_read(req, rsp);
      
   default:
      fprintf(stderr, "handle_request: unknown type %d\n", req->mreq_type);
      return HS_DEL;
   }
}

/* Two types of request handling functions: (i) those that require responses, 
 * and (ii) those that do not. */


static enum handler_e handle_request_read(const struct middfs_request *req,
                                          struct middfs_response *rsp) {
   enum handler_e retv = HS_DEL;
   char *path = NULL;
   int fd = -1;
   char *buf;
   
   
   /* get file path */
   path = middfs_localpath_tmp(req->mreq_rsrc.mr_path);

   /* open file */
   if ((fd = open(path, O_RDONLY)) < 0) {
      perror("open");
      goto cleanup;
   }

   /* get read info */
   off_t offset = req->mreq_off;
   size_t size = req->mreq_size;

   /* allocate buffer */
   if ((buf = malloc(size)) == NULL) {
      perror("malloc");
      goto cleanup;
   }

   /* read */
   ssize_t bytes_read;
   if ((bytes_read = pread(fd, buf, size, offset)) < 0) {
      perror("pread");
      goto cleanup;
   }
   
   /* construct response */
   rsp->nbytes = bytes_read;
   rsp->data = buf;
   
   /* success */
   retv = HS_SUC;
   
 cleanup:
   free(path);
   if (retv == HS_DEL) {
      free(buf);
   }
   if (fd >= 0) {
      close(fd);
   }
   
   return retv;
}
