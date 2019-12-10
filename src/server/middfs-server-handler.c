/* middfs-server-handler.c -- implementation of server-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "server/middfs-client.h"
#include "server/middfs-server-handler.h"
#include "server/middfs-server.h"

static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_pkt_wr_fin(struct middfs_sockinfo *sockinfo);
static enum handler_e handle_req_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_req_wr_fin(struct middfs_sockinfo *sockinfo);
static enum handler_e handle_rsp_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_rsp_wr_fin(struct middfs_sockinfo *sockinfo);
static enum handler_e handle_req_rd_fin_root(struct middfs_sockinfo *sockinfo,
                                             const struct middfs_request *req,
                                             struct middfs_response *rsp);


static enum handler_e handle_connect(struct middfs_sockinfo *sockinfo,
                                     const struct middfs_packet *in_pkt);

static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
				    const struct middfs_packet *in_pkt) {

   switch (in_pkt->mpkt_type) {
   case MPKT_CONNECT:
      return handle_connect(sockinfo, in_pkt);
      
   case MPKT_DISCONNECT:
      /* TODO */
      abort();
      
   case MPKT_REQUEST:
      return handle_req_rd_fin(sockinfo, in_pkt);
      
   case MPKT_RESPONSE:
      return handle_rsp_rd_fin(sockinfo, in_pkt);
      
   case MPKT_NONE:
   default:
      fprintf(stderr, "handle_pkt_rd_fin: received packet with invalid type %d\n",
              in_pkt->mpkt_type);
      return HS_DEL; /* invalid packet*/
   }
}

static enum handler_e handle_pkt_wr_fin(struct middfs_sockinfo *sockinfo) {
   switch (sockinfo->state) {
   case MSS_REQFWD:
      return handle_req_wr_fin(sockinfo);
      
   case MSS_RSPWR:
      return handle_rsp_wr_fin(sockinfo);

   default:
      fprintf(stderr, "handle_pkt_wr_fin: unexpected socket state %d\n", sockinfo->state);
      return HS_DEL;
   }
}



/* Packet-type specific handlers */
static enum handler_e handle_req_rd_fin_peer(struct middfs_sockinfo *sockinfo,
                                             const struct middfs_packet *in_pkt,
                                             struct middfs_packet *out_pkt);

static enum handler_e handle_req_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt) {
   enum handler_e retv;
   struct middfs_packet out_pkt;

   /* validate resource */
   const struct middfs_request *req = &in_pkt->mpkt_un.mpkt_request;
   const struct rsrc *rsrc = &req->mreq_rsrc;
   const char *owner = rsrc->mr_path;
   if (owner == NULL || *owner == '\0') {
      /* root resource */
      packet_init(&out_pkt, MPKT_RESPONSE);
      retv = handle_req_rd_fin_root(sockinfo, &in_pkt->mpkt_un.mpkt_request,
                                    &out_pkt.mpkt_un.mpkt_response);
   } else {
      /* peer resource */
      retv = handle_req_rd_fin_peer(sockinfo, in_pkt, &out_pkt);
   }
   
   if (retv == HS_DEL) {
      return retv;
   }
   
   if (buffer_serialize(&out_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
      perror("buffer_serialize");
      return HS_DEL;
   }
      
   return HS_SUC;
}

/* handle_req_rd_fin_root() -- handle request for resources owned by root 
 */
static enum handler_e handle_req_rd_fin_root(struct middfs_sockinfo *sockinfo,
                                             const struct middfs_request *req,
                                             struct middfs_response *rsp) {
   switch (req->mreq_type) {
   case MREQ_READDIR:
      response_init(rsp, MRSP_DIR);
      if (clients_readdir(&clients, &rsp->mrsp_un.mrsp_dir) < 0) {
         response_error(rsp, errno);
      }
      break;

   case MREQ_ACCESS:
      {
         int mode = req->mreq_mode;
         if ((mode & (R_OK | W_OK | X_OK)) != mode) {
            /* invalid bits are present */
            response_error(rsp, EINVAL);
         } else if ((mode & R_OK) == mode) {
            response_init(rsp, MRSP_OK); /* just asking for read permissions, so OK */
         } else {
            response_error(rsp, EACCES); /* asking for write or execute; permission denied */
         }
      }
      break;
      
   default:
      fprintf(stderr, "handle_req_rd_fin_root: request type not implemented\n");
      response_error(rsp, ENOENT);
      break;
   }
   
   sockinfo->state = MSS_RSPWR;
   sockinfo->out.fd = sockinfo->in.fd;
   sockinfo->in.fd = -1;
   
   return HS_SUC;
}

/* handle_req_rd_fin_peer() -- handle requests for resources owned by peers 
 */
static enum handler_e handle_req_rd_fin_peer(struct middfs_sockinfo *sockinfo,
                                             const struct middfs_packet *in_pkt,
                                             struct middfs_packet *out_pkt) {
   int tmpfd;
   
   /* check if client is online */
   const char *recipient_name = in_pkt->mpkt_un.mpkt_request.mreq_rsrc.mr_owner;
   const struct client *recipient_info;
   if ((recipient_info = client_find(recipient_name, &clients)) == NULL) {
      packet_error(out_pkt, ENOENT);
      
      fprintf(stderr, "client_find: client ``%s'' not found\n", recipient_name);

      sockinfo->state = MSS_RSPWR;
      sockinfo->out.fd = sockinfo->in.fd;
      sockinfo->in.fd = -1;
   } else {
      sockinfo->state = MSS_REQFWD; /* exclusively forward for now. */   
      *out_pkt = *in_pkt;
      
      /* open connection with client responder */
      if ((tmpfd = inet_connect(recipient_info->IP, recipient_info->port)) < 0) {
         perror("inet_connect");
         return HS_DEL;
      }
      sockinfo->out.fd = tmpfd;
   }

   return HS_SUC;
}



static enum handler_e handle_req_wr_fin(struct middfs_sockinfo *sockinfo) {
   assert(sockinfo->state == MSS_REQFWD);

   int tmpfd;
   
   sockinfo->state = MSS_RSPFWD; /* now forward response */
   tmpfd = sockinfo->out.fd; /* invert polarity of socket */
   sockinfo->out.fd = sockinfo->in.fd;
   sockinfo->in.fd = tmpfd;
   
   return HS_SUC;
}

static enum handler_e handle_rsp_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt) {
   assert(sockinfo->state == MSS_RSPFWD);

   sockinfo->state = MSS_RSPWR;
   if (buffer_serialize(in_pkt, (serialize_f) serialize_pkt, &sockinfo->out.buf) < 0) {
      perror("buffer_serialize");
      return HS_DEL;
   }

   return HS_SUC;
}

static enum handler_e handle_rsp_wr_fin(struct middfs_sockinfo *sockinfo) {
   assert(sockinfo->state == MSS_RSPWR);

   sockinfo->state = MSS_CLOSED; /* this socket will be closed */
   
   return HS_DEL;   
}




static enum handler_e handle_connect(struct middfs_sockinfo *sockinfo,
                                     const struct middfs_packet *in_pkt) {

   const struct middfs_connect *conn = &in_pkt->mpkt_un.mpkt_connect;
   
   /* create client */
   struct client client;
   if (client_create(conn, sockinfo->in.fd, &client) < 0) {
      perror("client_create");
      return HS_DEL;
   }

   /* insert into clients list */
   if (clients_add(&client, &clients) < 0) {
      perror("clients_add");
      return HS_DEL;
   }

   client_print(&client);

   return HS_DEL;
}



/* Handler Interface Definition */

struct handler_info server_hi =
  {.rd_fin = handle_pkt_rd_fin,
   .wr_fin = handle_pkt_wr_fin
  };
