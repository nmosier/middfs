/* middfs-client-rsrc.c -- implementation of utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "middfs-client-rsrc.h"
#include "middfs-client.h"

/* utility function definitions */

#if 0
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
#endif

char *middfs_localpath_tmp(const char *middfs_path) {
  char *localpath;
  asprintf(&localpath, "%s/%s%s", middfs_conf.local_dir,
	   middfs_conf.client_name, middfs_path);
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


/* middfs_rsrc_create() -- construct resource from path
 * ARGS:
 * RETV: returns 0 on success; -errno on error.
 */
int middfs_rsrc_create(const char *path, struct middfs_rsrc *rsrc) {
  /* find owner string in _path_ */
  const char *owner_begin = path + strspn(path, "/");

  if (owner_begin == path) {
    return -EINVAL; /* _path_ is relative (no leading '/') */
  }

  if (*owner_begin == '\0') {
    /* requesting middfs root -- special type of request */
    rsrc->mr_owner = rsrc->mr_path = NULL;
    rsrc->mr_type = MR_ROOT;
  } else {
    /* requesting something in a user directory */
    
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

  }
  
  /* initialize other fields */
  rsrc->mr_fd = -1;
  
  return 0; /* success */
}

int middfs_rsrc_delete(struct middfs_rsrc *rsrc) {
  int retv = 0;

  if (rsrc != NULL) {
    if (rsrc->mr_fd >= 0) {
      if (close(rsrc->mr_fd) < 0) {
	retv = -errno;
      }
    }
    
    free(rsrc->mr_owner);
    free(rsrc->mr_path);
  }
  
  return retv;
}

int middfs_rsrc_open(struct middfs_rsrc *rsrc, int flags, ...) {
  va_list args; /* only needed if (flags & O_CREAT) -- will contain
		 * _mode_ param (for more info man open(2))*/
  int retv = 0;
  int fd = -1;
  int mode = 0; /* NOTE: This might need to be init'ed to umask(2)? */
  char *localpath = NULL;

  /* check for file create flag; obtain mode param if present */
  if ((flags & O_CREAT)) {
    va_start(args, flags);
    mode = va_arg(args, int);
    va_end(args);
  }
  
  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    /* open local file */
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if ((fd = open(localpath, flags, mode)) < 0) {
      retv = -errno;
    } else {
      rsrc->mr_fd = fd;
    }
    free(localpath);
    return retv;
    
  default:
    abort();
  }
}

int middfs_rsrc_lstat(const struct middfs_rsrc *rsrc,
		      struct stat *sb) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (lstat(localpath, sb) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_readlink(const struct middfs_rsrc *rsrc,
			 char *buf, size_t bufsize) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if ((retv = readlink(localpath, buf, bufsize)) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_access(const struct middfs_rsrc *rsrc, int mode) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (access(localpath, mode) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_mkdir(const struct middfs_rsrc *rsrc, mode_t mode) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (mkdir(localpath, mode) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_unlink(const struct middfs_rsrc *rsrc) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (unlink(localpath) < 0) {
      retv = -errno;
    }
    free(localpath);
    break;
    
  default:
    abort();
  }

  return retv;
}

int middfs_rsrc_rmdir(const struct middfs_rsrc *rsrc) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (rmdir(localpath) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;
    
  default:
    abort();
  }
}


int middfs_rsrc_truncate(const struct middfs_rsrc *rsrc, off_t size) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    if (rsrc->mr_fd >= 0) {
      if (ftruncate(rsrc->mr_fd, size) < 0) {
	retv = -errno;
      }
    } else {
      localpath = middfs_localpath_tmp(rsrc->mr_path);
      if (truncate(localpath, size) < 0) {
	retv = -errno;
      }
      free(localpath);
    }
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_symlink(const struct middfs_rsrc *rsrc,
			const char *to) {
  int retv = 0;
  char *localpath = NULL;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(rsrc->mr_path);
    if (symlink(to, localpath) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int middfs_rsrc_rename(const struct middfs_rsrc *from,
		       const struct middfs_rsrc *to) {
  int retv = 0;
  
  if (from->mr_type == MR_LOCAL && to->mr_type == MR_LOCAL) {
    char *local_to, *local_from;
    local_from = middfs_localpath_tmp(from->mr_path);
    local_to = middfs_localpath_tmp(to->mr_path);

    if (rename(local_from, local_to) < 0) {
      retv = -errno;
    }

    free(local_to);
    free(local_from);
    return retv;
  } else {
    return -EOPNOTSUPP;
  }
}

int middfs_rsrc_chmod(const struct middfs_rsrc *rsrc, mode_t mode) {
  int retv = 0;
  char *localpath;

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    if (rsrc->mr_fd >= 0) {
      if (fchmod(rsrc->mr_fd, mode) < 0) {
	retv = -errno;
      }
    } else {
      localpath = middfs_localpath_tmp(rsrc->mr_path);
      if (lchmod(localpath, mode) < 0) {
	retv = -errno;
      }
      free(localpath);
    }
    return retv;
    
  default:
    abort();
  }
  
}
