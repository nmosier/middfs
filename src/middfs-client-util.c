/* middfs-client-util.c -- implementation of utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>

#include "middfs-client-util.h"
#include "middfs-client.h"

/* utility function definitions */

/* middfs_localpath() -- get path of middfs path in local file
 * system.
 * ARGS:
 *  - middfs_path: file path in middfs space; always starts with '/'
 * RETV: malloc(3)ed pointer to corresponding local path on local 
 *       disk. Note that this must be free(3)ed.
 */
char *middfs_localpath(const char *middfs_path) {
  char *localpath;
  const char *fmt = (middfs_path[0] == '/') ? "%s%s" : "%s/%s";
  
  asprintf(&localpath, fmt, middfs_conf.local_dir, middfs_path);
  return localpath;
}

/* middfs_abspath() -- convert a relative path into an absolute path
 * ARGS:
 *  - path: a pointer to a dynamically allocated string
 * RETV: the absolute path is placed at *path (i.e. it is modified
 *       in-place). The original string at *path is freed.
 *       Upon success, 0 is returned; otherwise, -errno is returned.
 */
int middfs_abspath(char **path) {
  char *cwd, *relpath;

  switch ((*path)[0]) {
  case '/': /* already an absolute path */
    return 0;
    
  case '\0': /* empty string is not a valid path */
    return -EINVAL;

  default: /* prepend current working directory to _path_ */
    if ((cwd = getcwd(NULL, 0)) == NULL) {
      return -errno;
    }
    relpath = *path;
    asprintf(path, "%s/%s", cwd, relpath);
    free(relpath);
    return 0;
  }
}


/* middfs_rsrc() -- construct resource from path
 * ARGS:
 * RETV: returns 0 on success; -errno on error.
 */
int middfs_rsrc(const char *path, struct middfs_rsrc *rsrc) {
  /* find owner string in _path_ */
  const char *owner_begin = path + strspn(path, "/");

  if (owner_begin == path) {
    return -EINVAL; /* _path_ is relative (no leading '/') */
  }

  if (*owner_begin == '\0') {
    /* requesting middfs root -- special type of request */
    rsrc->mr_owner = rsrc->mr_path = NULL;
    rsrc->mr_type = MR_ROOT;
    return 0;
  }

  const char *owner_end = owner_begin + strcspn(owner_begin, "/");
  
  /* dup and save owner string */
  rsrc->mr_owner = strndup(owner_begin, owner_end - owner_begin);
  
  /* find owner path string */
  if (*owner_end == '\0') {
    rsrc->mr_path = strdup("/");
  } else {
    rsrc->mr_path = strdup(owner_end);
  }

  /* find resource type */
  if (strcmp(rsrc->mr_owner, middfs_conf.client_name) != 0) {
    rsrc->mr_type = MR_NETWORK; /* resource not owned by client */
  } else {
    rsrc->mr_type = MR_LOCAL; /* resource owned by client */
  }
  
  return 0; /* success */
}
