/* middfs-handle.h -- middfs  handler module 
 * Nicholas Mosier & Tommaso Monaco 2019 
 */

#ifndef __MIDDFS_HANDLE_H
#define __MIDDFS_HANDLE_H

#include <stdio.h>

#include "middfs-serial.h"

/* Handler function return value enum */
enum handler_e {
   HS_ERR = -1, /* An internal error occurred. */
   HS_SUC,      /* Nothing special. */
   HS_NEW,      /* Create new socket.  */
   HS_DEL,      /* An error has occurred during handling of packet; socket should be deleted. */
};

/* Handler function type */
typedef int (*handler_f)(const struct middfs_packet *in, struct middfs_packet *out);

/* struct handler_info -- this contains all the information that the middfs Connection Manager
 * needs to handle incoming data. This generally involves
 *   I. Incoming Data
 *      (i) determining how to interpret incoming data
 *          (e.g. what data type should it be deserialized as?).
 *     (ii) providing the deserialization function for incoming data.
 *    (iii) providing the function to handle 
 *  II. Outgoing Data
 *  (ii) providing the deserialization function for 
 * TODO
 */ 
struct handler_info {
  /* function pointer that handles the value deserialized by _in_deserial_*/
   handler_f handle_in;
   handler_f handle_out;
};

#endif
