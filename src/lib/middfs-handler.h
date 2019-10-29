/* middfs-handle.h -- middfs  handler module 
 * Nicholas Mosier & Tommaso Monaco 2019 
 */

#ifndef __MIDDFS_HANDLE_H
#define __MIDDFS_HANDLE_H

#include <stdio.h>

#include "middfs-serial.h"

/* Handler function type */
typedef int (*handler_f)(const void *in, void *out);

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
  /* function pointer for deserializing incoming data. */
  deserialize_f in_deserial;

  /* size of struct being deserialized from input */
  size_t in_size;

  /* function pointer that handles the value deserialized by _in_deserial_*/
  handler_f in_handler;

  /* function pointer for serializing outgoing struct */
  serialize_f out_serial;

  /* size of struct being serialized to output */
  size_t out_size; 
  
};

#endif