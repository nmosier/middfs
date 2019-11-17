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
  
  /* (none): getattr, unlink, getattr, rmdir, readdir */
};
bool req_has_mode(enum middfs_request_type type);
bool req_has_size(enum middfs_request_type type);
bool req_has_to(enum middfs_request_type type);
bool req_has_off(enum middfs_request_type type);

/* TODO: response will definintely need to be changed in the future. */
struct middfs_response {
   uint64_t nbytes;
   void *data;
};

struct middfs_connect {
   char *name; /* username of client */
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

// typedef int (*packet_handle_f)(struct middfs_packet *pkt);

int packet_handle_none(struct middfs_packet *pkt);
int packet_handle_connect(struct middfs_packet *pkt);
int packet_handle_disconnect(struct middfs_packet *pkt);
int packet_handle_request(struct middfs_packet *pkt);


#endif
