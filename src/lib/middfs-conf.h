/* middfs-conf.h -- configuration file manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_CONF_H
#define __MIDDFS_CONF_H

#include <stdio.h>

/* middfs configuration entry: key-value pair 
 * NOTE: all strings are allocated. 
 */
struct middfs_conf_kv {
  char *key;
  char *val;
};

int conf_create(const char *confpath, struct middfs_conf_kv kvs[], size_t kvcount);
void conf_delete(struct middfs_conf_kv kvs[], size_t kvcount);


#endif
