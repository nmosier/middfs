/* middfs-conf.h -- configuration file manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_CONF_H
#define __MIDDFS_CONF_H

#include <stdio.h>

char *conf_get(const char *name);
int conf_set(const char *name, const char *value, int overwrite);
int conf_put(const char *string);
int conf_unset(const char *name);
int conf_load(const char *path);

#endif
