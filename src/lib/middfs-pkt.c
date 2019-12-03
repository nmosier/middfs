/* middfs-pkt.c -- middfs packet handler
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <errno.h>
#include <stdbool.h>

#include "lib/middfs-pkt.h"
#include "lib/middfs-buf.h"

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

/* packet_send() -- send a packet over the given socket fd 
 * ARGS:
 *  - fd: socket file descriptor to write packet over 
 *  - pkt: packet to send 
 * RETV: 0 on success; negated error code on error.
 */
int packet_send(int fd, const struct middfs_packet *pkt) {
   int retv = 0;
   struct buffer buf;
   buffer_init(&buf);

   if (buffer_serialize(pkt, (serialize_f) serialize_pkt, &buf) < 0) {
      return -errno;
   }
   while (!buffer_isempty(&buf)) {
      int write_retv;
      if ((write_retv = buffer_write(fd, &buf)) < 0 && write_retv != EINTR) {
         retv = -errno;
         break;
      }
   }

   /* cleanup */
   buffer_delete(&buf);

   return retv;
}

/* packet_recv() -- receive a packet over the given socket fd
 * ARGS:
 *  - fd: socket file descriptor to receive packet over
 *  - pkt: pointer to packet to deserialize into 
 * RETV: 0 on success; negated error code on error.
 */
int packet_recv(int fd, struct middfs_packet *pkt) {
   int retv = 0;
   struct buffer buf;
   buffer_init(&buf);
   
   /* deserialize & read into buffer */
   while ((retv = buffer_deserialize(pkt, (deserialize_f) deserialize_pkt, &buf)) > 0) {
      int read_retv;
      /* NOTE: Be careful to not treat interrupt as error. */
      if ((read_retv = buffer_read(fd, &buf)) < 0 && read_retv != EINTR) {
         retv = -errno;
         goto cleanup;
      }
   }
   if (retv < 0) {
      retv = -errno;
   }
   
 cleanup:
   buffer_delete(&buf);
   
   return retv;
}
            
