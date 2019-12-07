/* middfs-pkt.h -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_REQ_H
#define __MIDDFS_REQ_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "middfs-rsrc.h"

#define MPKT_MAGIC 1800

enum middfs_packet_type
  {MPKT_NONE,
   MPKT_CONNECT,
   MPKT_DISCONNECT,
   MPKT_REQUEST,
   MPKT_RESPONSE, /* TODO: Write response */
   MPKT_NTYPES /* counts number of types */
  };

enum middfs_request_type
  {MREQ_NONE,
   MREQ_GETATTR,
   MREQ_ACCESS,
   MREQ_READLINK,
   MREQ_MKDIR,
   MREQ_SYMLINK,
   MREQ_UNLINK,
   MREQ_RMDIR,
   MREQ_RENAME,
   MREQ_CHMOD,
   MREQ_TRUNCATE,
   MREQ_OPEN,
   MREQ_CREATE,
   MREQ_READ,
   MREQ_WRITE,
   MREQ_READDIR,
   MREQ_NTYPES /* counts number of types */
  };

/* TODO:  write serializer & deserializer for these structs */
struct middfs_request {
  enum middfs_request_type mreq_type;
  
  char *mreq_requester;

  struct rsrc mreq_rsrc;

  /* request-specific members */
  int mreq_mode;    /* access, chmod, create, open */
  uint64_t mreq_size; /* readlink, truncate, read, write */
  char *mreq_to;    /* symlink, rename */
  uint64_t mreq_off;  /* read, write */
   void *mreq_data; /* write */
  
  /* (none): getattr, unlink, getattr, rmdir, readdir */
};
bool req_has_mode(enum middfs_request_type type);
bool req_has_size(enum middfs_request_type type);
bool req_has_to(enum middfs_request_type type);
bool req_has_off(enum middfs_request_type type);
bool req_has_data(enum middfs_request_type type);

struct middfs_stat {
   uint64_t mstat_size;
   uint64_t mstat_blocks;
   uint64_t mstat_blksize;
};

struct middfs_data {
   void *mdata_buf;
   uint64_t mdata_nbytes;
};

enum middfs_response_type
   {MRSP_OK,
    MRSP_DATA,
    MRSP_STAT,
    MRSP_ERROR
   };

struct middfs_response {
   enum middfs_response_type mrsp_type;
   
   union {
      struct middfs_data mrsp_data;           /* read */
      int32_t mrsp_error;                     /* error */
      struct middfs_stat mrsp_stat;           /* getattr */
   } mrsp_un;
};

/* CONNECT PACKET
 * A client sends this packet to the server to connect, i.e. check in.
 * The client can specify their preferred username.
 * The server responds with a connect packet confirming the client's connection
 * and responds with the client's actual username. 
 */
struct middfs_connect {
   char *name; /* username of client */
   uint32_t port; /* port on which to connect to client responder */
};

struct middfs_disconnect {
  /* TODO: stub */
  int dummy;
};

struct middfs_packet {
  uint32_t mpkt_magic;
  enum middfs_packet_type mpkt_type;
  union {
    struct middfs_request mpkt_request;
     struct middfs_response mpkt_response;
    struct middfs_connect mpkt_connect;
    struct middfs_disconnect mpk_disconnect;
  } mpkt_un;
};

int packet_handle_none(struct middfs_packet *pkt);
int packet_handle_connect(struct middfs_packet *pkt);
int packet_handle_disconnect(struct middfs_packet *pkt);
int packet_handle_request(struct middfs_packet *pkt);

/* Blocking Packet I/O */
int packet_send(int fd, const struct middfs_packet *pkt);
int packet_recv(int fd, struct middfs_packet *pkt);

#endif
