/* middfs-client-pkt.h */

#ifndef __MIDDFS_CLIENT_PKT_H
#define __MIDDFS_CLIENT_PKT_H

#include "lib/middfs-pkt.h"

int packet_send(int fd, const struct middfs_packet *pkt);
int packet_recv(int fd, struct middfs_packet *pkt);
int packet_xchg(const struct middfs_packet *out_pkt, struct middfs_packet *in_pkt);
int response_validate(const struct middfs_packet *pkt, enum middfs_response_type type);

#endif
