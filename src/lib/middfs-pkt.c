/* middfs-pkt.c -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "lib/middfs-pkt.h"
#include "lib/middfs-buf.h"
#include "lib/middfs-conf.h"
#include "lib/middfs-rsrc.h"

#include "client/middfs-client-conf.h"

bool req_has_mode(enum middfs_request_type type) {
  return type == MREQ_ACCESS || type == MREQ_CHMOD || type == MREQ_CREATE || type == MREQ_OPEN;
}

bool req_has_size(enum middfs_request_type type) {
  return type == MREQ_READLINK || type == MREQ_TRUNCATE || type == MREQ_READ || type == MREQ_WRITE;
}

bool req_has_to(enum middfs_request_type type) {
  return type == MREQ_SYMLINK || type == MREQ_RENAME;
}

bool req_has_off(enum middfs_request_type type) {
  return type == MREQ_READ || type == MREQ_WRITE;
}

bool req_has_data(enum middfs_request_type type) {
   return type == MREQ_WRITE;
}

void response_error(struct middfs_response *rsp, int error) {
   rsp->mrsp_type = MRSP_ERROR;
   rsp->mrsp_un.mrsp_error = error;
}

void packet_error(struct middfs_packet *pkt, int error) {
   pkt->mpkt_magic = MPKT_MAGIC;
   pkt->mpkt_type = MPKT_RESPONSE;

   struct middfs_response *rsp = &pkt->mpkt_un.mpkt_response;
   rsp->mrsp_type = MRSP_ERROR;
   rsp->mrsp_un.mrsp_error = error;
}


/* PACKET SUBTYPE INITIALIZATION FUNCTIONS */

void request_init(struct middfs_request *req, enum middfs_request_type type,
                  const struct rsrc *rsrc) {
   req->mreq_type = type;
   req->mreq_requester = conf_get(MIDDFS_CONF_USERNAME);
   req->mreq_rsrc = *rsrc;
}

void response_init(struct middfs_response *rsp, enum middfs_response_type type) {
   rsp->mrsp_type = type;
}

int connect_init(struct middfs_connect *conn) {
   int err = 0;
   
   conn->name = conf_get(MIDDFS_CONF_USERNAME);
   conn->port = conf_get_uint32(MIDDFS_CONF_LOCALPORT, &err);

   if (err) {
      return -1;
   }
   return 0;
}

/* TODO: Disconnect. */

/* PACKET INITIALIZATION FUNCTIONS */

/* packet_init() -- initialize bare packet */
void packet_init(struct middfs_packet *pkt, enum middfs_packet_type type) {
   pkt->mpkt_magic = MPKT_MAGIC;
   pkt->mpkt_type = type;
}


/* PACKET PRINTING */

const char *req_type_strs[MREQ_NTYPES] =
   {[MREQ_NONE] = "MREQ_NONE",
    [MREQ_GETATTR] = "MREQ_GETATTR",
    [MREQ_ACCESS] = "MREQ_ACCESS",
    [MREQ_READLINK] = "MREQ_READLINK",
    [MREQ_MKDIR] = "MREQ_MKDIR",
    [MREQ_SYMLINK] = "MREQ_SYMLINK",
    [MREQ_UNLINK] = "MREQ_UNLINK",
    [MREQ_RMDIR] = "MREQ_RMDIR",
    [MREQ_RENAME] = "MREQ_RENAME",
    [MREQ_CHMOD] = "MREQ_CHMOD",
    [MREQ_TRUNCATE] = "MREQ_TRUNCATE",
    [MREQ_OPEN] = "MREQ_OPEN",
    [MREQ_CREATE] = "MREQ_CREATE",
    [MREQ_READ] = "MREQ_READ",
    [MREQ_WRITE] = "MREQ_WRITE",
    [MREQ_READDIR] = "MREQ_READDIR",
   };


void print_request(const struct middfs_request *req) {
   fprintf(stderr, "{");
   enum middfs_request_type type = req->mreq_type;
   fprintf(stderr, ".mreq_type = %s, ", (type >= 0 && type < MREQ_NTYPES) ?
           req_type_strs[type] :  "<invalid>");
   fprintf(stderr, ".mreq_requester = ``%s'', ", req->mreq_requester);
   fprintf(stderr, ".mreq_rsrc = ");
   print_rsrc(&req->mreq_rsrc);
   fprintf(stderr, ", ");

   if (req_has_mode(type)) {
      fprintf(stderr, ".mreq_mode = %o, ", req->mreq_mode);
   }
   if (req_has_size(type)) {
      fprintf(stderr, ".mreq_size = %llu, ", (unsigned long long) req->mreq_size);
   }
   if (req_has_to(type)) {
      fprintf(stderr, ".mreq_to = ");
      print_rsrc(&req->mreq_to);
      fprintf(stderr, ", ");
   }
   if (req_has_off(type)) {
      fprintf(stderr, ".mreq_off = %llu, ", (unsigned long long) req->mreq_size);
   }
   if (req_has_data(type)) {
      fprintf(stderr, ".mreq_data = %p, ", (const void *) req->mreq_data);
   }

   fprintf(stderr, "}");
}


const char *rsp_type_strs[MRSP_NTYPES] =
   {[MRSP_OK] = "MRSP_OK",
    [MRSP_DATA] = "MRSP_DATA",
    [MRSP_STAT] = "MRSP_STAT",
    [MRSP_DIR] = "MRSP_DIR",
    [MRSP_ERROR] = "MRSP_ERROR",
   };

void print_response(const struct middfs_response *rsp) {
   enum middfs_response_type type = rsp->mrsp_type;
   fprintf(stderr, "{.mrsp_type = %s, ", (type >= 0 && type < MRSP_NTYPES) ?
           rsp_type_strs[type] : "<invalid type>");
   switch (type) {
   case MRSP_DATA:
      fprintf(stderr, ".mrsp_data = ");
      print_data(&rsp->mrsp_un.mrsp_data);
      break;
   case MRSP_STAT:
      fprintf(stderr, ".mrsp_stat = ");
      print_stat(&rsp->mrsp_un.mrsp_stat);
      break;
   case MRSP_DIR:
      fprintf(stderr, ".mrsp_dir = ");
      print_dir(&rsp->mrsp_un.mrsp_dir);
      break;
   case MRSP_ERROR:
      fprintf(stderr, ".mrsp_error = %d", rsp->mrsp_un.mrsp_error);
      break;
   case MRSP_OK:
   default:
      break;
   }
   fprintf(stderr, "}");
}


void print_data(const struct middfs_data *data) {
   fprintf(stderr, "{.mdata_nbytes = %llu, .mdata_buf = %p}",
           (unsigned long long) data->mdata_nbytes, (const void *) data->mdata_buf);
}

void print_stat(const struct middfs_stat *st) {
   fprintf(stderr, "{.mstat_mode = %o, .mstat_size = %llu, .mstat_blocks = %llu, " \
           ".mstat_blksize = %llu}", st->mstat_mode, st->mstat_size, st->mstat_blocks,
           st->mstat_blksize);
}

void print_dirent(const struct middfs_dirent *de) {
   fprintf(stderr, "{.mde_name = ``%s'', .mde_mode = %o}", de->mde_name, de->mde_mode);
}

void print_dir(const struct middfs_dir *dir) {
   fprintf(stderr, "{.mdir_count = %llu, .mdir_ents = {", dir->mdir_count);
   for (uint64_t i = 0; i < dir->mdir_count; ++i) {
      fprintf(stderr, "[%llu] = ", i);
      print_dirent(dir->mdir_ents + i);
      fprintf(stderr, ", ");
   }
   fprintf(stderr, "}}");
}


void print_connect(const struct middfs_connect *conn) {
   fprintf(stderr, "{.name = ``%s'', .port = %d}", conn->name, conn->port);
}


const char *packet_type_strs[MPKT_NTYPES] =
   {[MPKT_NONE] = "MPKT_NONE",
    [MPKT_CONNECT] = "MPKT_CONNECT",
    [MPKT_DISCONNECT] = "MPKT_DISCONNECT",
    [MPKT_REQUEST] = "MPKT_REQUEST",
    [MPKT_RESPONSE] = "MPKT_RESPONSE",
   };

void print_packet(const struct middfs_packet *pkt) {
   enum middfs_packet_type type = pkt->mpkt_type;
   fprintf(stderr, "{.mpkt_magic = %u, .mpkt_type = %s, ",
           pkt->mpkt_magic, (type >= 0 && type < MPKT_NTYPES) ?
           packet_type_strs[type] : "<invalid type>");
   switch (type) {
   case MPKT_CONNECT:
      print_connect(&pkt->mpkt_un.mpkt_connect);
      break;
   case MPKT_REQUEST:
      print_request(&pkt->mpkt_un.mpkt_request);
      break;
   case MPKT_RESPONSE:
      print_response(&pkt->mpkt_un.mpkt_response);
      break;
   case MPKT_DISCONNECT:
   case MPKT_NONE:
   default:
      break;
   }
}
