/* middfs-rsrc.c
 * Nicholas Mosier & Tommaso Monaco
 * 2019
 */

#include <string.h>
#include <stdio.h>

#include "middfs-util.h"
#include "middfs-rsrc.h"

void print_rsrc(const struct rsrc *rsrc) {
   fprintf(stderr, "{.mr_owner = ``%s'', .mr_path = ``%s''}", rsrc->mr_owner, rsrc->mr_path);
}
