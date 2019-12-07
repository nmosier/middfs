/* middfs-client-handler.c -- implementation of client-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

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

  struct middfs_packet out_pkt = {0};

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
  
  /* serialize response packet */
  if (buffer_serialize(&out_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
    perror("buffer_serialize");
    return HS_DEL;
  }

  /* Update state of socket for writing response */
  sockinfo->state = MSS_RSPWR;
  sockinfo->out.fd = sockinfo->in.fd;
  sockinfo->in.fd = -1;

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



static int handle_request_read(int fd, const struct middfs_request *req,
                               struct middfs_response *rsp);
static int handle_request_write(int fd, const struct middfs_request *req,
                                struct middfs_response *rsp);
static enum handler_e handle_request_getattr(const char *path, const struct middfs_request *req,
                                             struct middfs_response *rsp);

static handle_request_f handle_request_fns[MREQ_NTYPES] =
   {[MREQ_READ] = {.fd_f = handle_request_read},
    [MREQ_WRITE] = {.fd_f = handle_request_write},
   };


/* handle_request() -- handle a request and queue a response (if necessary).
 */
static enum handler_e handle_request(const struct middfs_packet *in_pkt,
                                     struct middfs_packet *out_pkt) {
   enum handler_e retv = HS_SUC;
   const struct middfs_request *req = &in_pkt->mpkt_un.mpkt_request;
   struct middfs_response *rsp = &out_pkt->mpkt_un.mpkt_response;
   char *path = NULL;
   int request_status;

   /* response packet setup */
   out_pkt->mpkt_magic = MPKT_MAGIC;
   out_pkt->mpkt_type = MPKT_RESPONSE;

   /* construct path */
   path = middfs_localpath_tmp(req->mreq_rsrc.mr_path);
   
   switch (req->mreq_type) {
   case MREQ_NONE:
      /* Then why the hell did you contact me? */
      fprintf(stderr, "handle_request: request of type ``MREQ_NONE''\n");
      retv = HS_DEL;
      break;

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
   case MREQ_READDIR:
      fprintf(stderr, "handle_request: request not implemented yet\n");
      retv = HS_DEL;
      break;

#if 1
   case MREQ_READ:
   case MREQ_WRITE:
      {
         int fd = -1;
         
         /* open file */
         if ((fd = open(path, O_RDONLY)) < 0) {
            fprintf(stderr, "open: ``%s'': %s\n", path, strerror(errno));
            request_status = -errno;
            break;
         }

         /* call handler */
         request_status = handle_request_fns[req->mreq_type].fd_f(fd, req, rsp);
         
         close(fd);
         break;
      }
      
      
#else
      
   case MREQ_READ:
      request_status = handle_request_read(path, req, rsp);
      break;

   case MREQ_WRITE:
      request_status = handle_request_write(path, req, rsp);
      break;
#endif

      
   default:
      fprintf(stderr, "handle_request: unknown type %d\n", req->mreq_type);
      retv = HS_DEL;
      break;
   }

   /* cleanup */
   free(path);

   if (retv != HS_SUC) {
      return retv;
   }

   /* process request status */
   if (request_status < 0) {
      /* error */
      rsp->mrsp_type = MRSP_ERROR;
      rsp->mrsp_un.mrsp_error = -request_status;
   }
   /* otherwise, leave as-is */
   
   return HS_SUC;
}

static int handle_request_read(int fd, const struct middfs_request *req,
                               struct middfs_response *rsp) {
   char *buf = NULL;

   /* get read info */
   off_t offset = req->mreq_off;
   size_t size = req->mreq_size;

   /* allocate buffer */
   if ((buf = malloc(size)) == NULL) {
      perror("malloc");
      return -errno;
   }
   
   /* read */
   ssize_t bytes_read;
   if ((bytes_read = pread(fd, buf, size, offset)) < 0) {
      rsp->mrsp_un.mrsp_error = errno;            
      perror("pread");
      free(buf);
      return -errno;
   }
   
   /* construct response */
   rsp->mrsp_type = MRSP_DATA;
   struct middfs_data *data = &rsp->mrsp_un.mrsp_data;
   data->mdata_buf = buf;
   data->mdata_nbytes = bytes_read;
   
   return 0;
}

static int handle_request_write(int fd, const struct middfs_request *req,
                                struct middfs_response *rsp) {
   char *buf = req->mreq_data;

   /* get write info */
   off_t offset = req->mreq_off;
   size_t size = req->mreq_size;

   /* write */
   ssize_t bytes_written;
   if ((bytes_written = pwrite(fd, buf, size, offset)) < 0) {
      perror("pwrite");
      return -errno;
   }
   
   /* construct response */
   rsp->mrsp_type = MRSP_OK;
   
   return 0;
}

static enum handler_e handle_request_getattr(const char *path, const struct middfs_request *req,
                                             struct middfs_response *rsp) {
   /* stat file */
   struct stat st;
   if (stat(path, &st) < 0) {
      fprintf(stderr, "stat: ``%s'': %s\n", path, strerror(errno));
      rsp->mrsp_type = MRSP_ERROR;
      rsp->mrsp_un.mrsp_error = errno;
   } else {
      /* construct response */
      rsp->mrsp_type = MRSP_STAT;
      struct middfs_stat *mstat = &rsp->mrsp_un.mrsp_stat;
      mstat->mstat_size = st.st_size;
      mstat->mstat_blocks = st.st_blocks;
      mstat->mstat_blksize = st.st_blksize;
   }

   return HS_SUC;
}
