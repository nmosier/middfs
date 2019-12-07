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

