/* middfs-server-handler.c -- implementation of server-specific packet handlers
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <assert.h>
#include <stdlib.h>

#include "lib/middfs-handler.h"
#include "lib/middfs-conn.h"

#include "server/middfs-client.h"
#include "server/middfs-server-handler.h"
#include "server/middfs-server.h"

#define CLIENTB "140.233.167.124" // nmosier's MacBook

static enum handler_e handle_pkt_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_pkt_wr_fin(struct middfs_sockinfo *sockinfo);
static enum handler_e handle_req_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_req_wr_fin(struct middfs_sockinfo *sockinfo);
static enum handler_e handle_rsp_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt);
static enum handler_e handle_rsp_wr_fin(struct middfs_sockinfo *sockinfo);

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
static enum handler_e handle_req_rd_fin(struct middfs_sockinfo *sockinfo,
                                        const struct middfs_packet *in_pkt) {
   assert(sockinfo->state == MSS_REQRD);

   int tmpfd;
   
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
   
   /* create client */
   struct client client;
   if (client_create(in_pkt->mpkt_un.mpkt_connect.name, sockinfo->in.fd, &client) < 0) {
      perror("client_create");
      return HS_DEL;
   }

   /* insert into clients list */
   if (clients_add(&client, &clients) < 0) {
      perror("clients_add");
      return HS_DEL;
   }

   /* DEBUG: print out info */
   fprintf(stderr, "new client connected: name = \"%s\", IP = \"%s\"\n", client.username,
           client.IP);

   return HS_DEL;
}



/* Handler Interface Definition */

struct handler_info server_hi =
  {.rd_fin = handle_pkt_rd_fin,
   .wr_fin = handle_pkt_wr_fin
  };



