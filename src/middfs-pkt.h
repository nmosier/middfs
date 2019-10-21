/* middfs-pkt.h -- shared structure for middfs requests
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_REQ_H
#define __MIDDFS_REQ_H

#include <stdint.h>

#define MPKT_MAGIC 1800

enum middfs_packet_type
  {MPKT_NONE,
   MPKT_CONNECT,
   MPKT_DISCONNECT,
   MPKT_REQUEST,
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

struct middfs_packet {
  uint32_t mpkt_magic;
  enum middfs_packet_type mpkt_type;
  union {
    struct middfs_request mpkt_request;
    // TODO -- add structs for other packet possibilities
  };
};

#endif
