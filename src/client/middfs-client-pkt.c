/* middfs-client-pkt.c -- packet utility functions specific to client requester
 * Nicholas Mosier, Tommaso Monaco 2019
 */

#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>

#include "lib/middfs-conf.h"
#include "lib/middfs-serial.h"
#include "lib/middfs-buf.h"
#include "lib/middfs-pkt.h"

#include "client/middfs-client-conf.h"
#include "client/middfs-client-pkt.h"

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

   fprintf(stderr, "FUSE: ");
   print_packet(pkt);
   
   if (buffer_serialize(pkt, (serialize_f) serialize_pkt, &buf) < 0) {
      return -errno;
   }
   while (!buffer_isempty(&buf)) {
      int write_retv;
      if ((write_retv = buffer_write(fd, &buf)) < 0 && errno != EINTR) {
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
      if ((read_retv = buffer_read(fd, &buf)) <= 0) {
         break;
      }
   }
   if (retv < 0) {
      retv = -errno;
   } else {
      fprintf(stderr,"RESP: ");
      print_packet(pkt);
   }
   
   /* cleanup */
   buffer_delete(&buf);
   
   return retv;
}
            
/* packet_xchg() -- exchange packets with server 
 * ARGS: */
int packet_xchg(const struct middfs_packet *out_pkt, struct middfs_packet *in_pkt) {
   int retv = 0;
   const char *serverip = conf_get(MIDDFS_CONF_SERVERIP);
   int err = 0;
   uint32_t serverport = conf_get_uint32(MIDDFS_CONF_SERVERPORT, &err);

   /* validate params */
   assert(serverip != NULL && err == 0);

   /* connect to server */
   int fd;
   if ((fd = inet_connect(serverip, serverport)) < 0) {
      return -errno;
   }

   /* send packet and receive response */
   if (packet_send(fd, out_pkt) < 0 || packet_recv(fd, in_pkt) < 0) {
      retv = -errno;
   }

   close(fd);
   return retv;
}


/* response_verify() -- verify that packet is response is of given type. 
 * ARGS:
 *  - pkt: packet to verify
 *  - type: expected type of response. */
int response_validate(const struct middfs_packet *pkt, enum middfs_response_type type) {
   if (pkt->mpkt_magic != MPKT_MAGIC) {
      return -EIO;
   }
   if (pkt->mpkt_type != MPKT_RESPONSE) {
      return -EIO;
   }
   const struct middfs_response *rsp = &pkt->mpkt_un.mpkt_response;
   if (rsp->mrsp_type != type) {
      if (rsp->mrsp_type == MRSP_ERROR) {
         return -rsp->mrsp_un.mrsp_error;
      } else {
         return -EIO;
      }
   }

   return 0;
}
