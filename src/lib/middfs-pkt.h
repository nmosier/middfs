/* middfs-pkt.h -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_REQ_H
#define __MIDDFS_REQ_H

#include <stdint.h>

#include "middfs-rsrc.h"

#define MPKT_MAGIC 1800

enum middfs_packet_type
  {MPKT_NONE,
   MPKT_CONNECT,
   MPKT_DISCONNECT,
   MPKT_REQUEST,
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
   MREQ_USERS,
  };

struct middfs_request req;

/* TODO:  write serializer & deserializer for these structs */
struct middfs_request {
  enum middfs_request_type mreq_type;
  
  char *mreq_requester;

  struct rsrc rsrc;
};

struct middfs_connect {
  /* TODO: stub */
};

struct middfs_disconnect {
  /* TODO: stub */
};

struct middfs_packet {
  uint32_t mpkt_magic;
  enum middfs_packet_type mpkt_type;
  union {
    struct middfs_request mpkt_request;
    struct middfs_connect mpkt_connect;
    struct middfs_disconnect mpk_disconnect;
    // TODO -- add structs for other packet possibilities
  } mpkt_un;
};

typedef int (*packet_handle_f)(struct middfs_packet *pkt);

int packet_handle_none(struct middfs_packet *pkt);
int packet_handle_connect(struct middfs_packet *pkt);
int packet_handle_disconnect(struct middfs_packet *pkt);
int packet_handle_request(struct middfs_packet *pkt);


#endif
