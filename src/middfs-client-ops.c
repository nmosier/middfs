/* middfs-client-ops.c -- implementation of middfs-client file
 * operations
 * Tommaso Monaco & Nicholas Mosier, Oct 2019 
 */

#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>

#include "middfs-client-ops.h"
#include "middfs-client-rsrc.h"

/* file I/O function definitions */

/* NOTE: return value is for private context and is passed
 * to all other middfs functions. Don't need to use for now,
 * though. */
static void *middfs_init(struct fuse_conn_info *conn,
			 struct fuse_config *cfg) {
  // cfg->kernel_cache = 1; /* no kernel caching? */
  cfg->use_ino = 1; /* use inodes */
  
  /* disable caching */
  cfg->entry_timeout = 0.0;
  cfg->attr_timeout = 0.0;
  cfg->negative_timeout = 0.0;
  
  return NULL;
}


static int middfs_getattr(const char *path, struct stat *stbuf,
			  struct fuse_file_info *fi) {
  int retv = 0;
  struct middfs_rsrc *rsrc = NULL;
  struct middfs_rsrc rsrc_tmp;
    
  if (fi == NULL) {
    /* create & open temporary resource */
    if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
      return retv;
    }
    rsrc = &rsrc_tmp;
  } else {
    rsrc = (struct middfs_rsrc *) fi->fh;
  }

  /* get lstat info */
  if ((retv = middfs_rsrc_lstat(rsrc, stbuf)) < 0) {
    goto cleanup;
  }

 cleanup:
  /* delete temporary resource if needed */
  if (fi == NULL) {
    int res = middfs_rsrc_delete(&rsrc_tmp);
    retv = (retv < 0) ? retv : res; /* correctly set return val */
  }
  
  return retv;
}

static int middfs_access(const char *path, int mode) {
  int retv = 0;
  int res;
  char *localpath = NULL;
  struct middfs_rsrc rsrc_tmp;

  /* create temporary resource */
  if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
    return retv;
  }

  if ((retv = middfs_rsrc_access(&rsrc_tmp, mode)) < 0) {
    goto cleanup;
  }

 cleanup:
  res = middfs_rsrc_delete(&rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  free(localpath);
  return retv;
}

static int middfs_readlink(const char *path, char *buf,
			   size_t size) {
  int retv = 0;
  int res;
  struct middfs_rsrc rsrc_tmp;
  
  if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
    return retv;
  }
  if ((retv = middfs_rsrc_readlink(&rsrc_tmp, buf, size)) < 0) {
    goto cleanup;
  }

  buf[retv] = '\0';
  
 cleanup:
  res = middfs_rsrc_delete(&rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  return retv;
}

static int middfs_readdir(const char *path, void *buf,
			  fuse_fill_dir_t filler, off_t offset,
			  struct fuse_file_info *fi,
			  enum fuse_readdir_flags flags) {
  DIR *dir = NULL;
  struct dirent *dir_entry;
  int retv = 0;
  struct middfs_rsrc rsrc_tmp;

  /* get resource handle */
  if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
    return retv;
  }
  if ((retv = middfs_rsrc_open(&rsrc_tmp, O_RDONLY)) < 0) {
    goto cleanup;
  }
  
  /* open directory */
  if ((dir = fdopendir(rsrc_tmp.mr_fd)) == NULL) {
    retv = -errno;
    goto cleanup;
  }
  
  while ((dir_entry = readdir(dir)) != NULL) {
    struct stat st;
    memset(&st, sizeof(st), 0);
    st.st_ino = dir_entry->d_ino;
    st.st_mode = dir_entry->d_type << 12; /* no clue... */
    if (filler(buf, dir_entry->d_name, &st, 0, 0)) {
      break; /* buffer full? */
    }
  }

 cleanup:
  if (dir != NULL) {
    if (closedir(dir) < 0) {
      if (retv >= 0) {
	retv = -errno;
      }
    }
  }
  
  return retv;
}


#if 0
static int middfs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int retv = 0;

  if (mknod_wrapper(AT_FDCWD, localpath, NULL,mode, rdev) < 0) {
    retv = -errno;
  }

  return retv;
}
#endif


static int middfs_mkdir(const char *path, mode_t mode) {
  int retv = 0;
  int res;
  struct middfs_rsrc rsrc_tmp;

  
  /* acquire temporary resource */
  if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
    return retv;
  }

  /* mkdir for resource */
  retv = middfs_rsrc_mkdir(&rsrc_tmp, mode);

  /* cleanup */
  res = middfs_rsrc_delete(&rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  
  return retv;
}

static int middfs_unlink(const char *path) {
  int retv = 0;
  int res;
  struct middfs_rsrc rsrc_tmp;
  
  /* acquire temporary resource */
  if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
    return retv;
  }
  retv = middfs_rsrc_unlink(&rsrc_tmp);

  res = middfs_rsrc_delete(&rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  
  return retv;
}

static int middfs_rmdir(const char *path) {
  int retv = 0;
  char *localpath = middfs_localpath(path);

  if (rmdir(localpath) < 0) {
    retv = -errno;
  }

  free(localpath);
  return retv;
}

static int middfs_symlink(const char *to, const char *from) {
  int retv = 0;
  char *local_to = middfs_localpath(to);
  char *local_from = middfs_localpath(from);
  
  if (symlink(to, local_from) < 0) {
    retv = -errno;
  }
  
  free(local_to);
  free(local_from);
  return retv;
}

static int middfs_rename(const char *from, const char *to,
			 unsigned int flags) {
  int retv = 0;
  char *local_to, *local_from;

  /* no flags are currently supported */
  if (flags) {
    return -EINVAL;
  }
  
  local_to = middfs_localpath(to);
  local_from = middfs_localpath(from);

  if (rename(local_from, local_to) < 0) {
    retv = -errno;
  }
  
  free(local_to);
  free(local_from);
  return retv;
}

static int middfs_link(const char *from, const char *to) {
  int retv = 0;
  char *local_from, *local_to;

  local_from = middfs_localpath(from);
  local_to = middfs_localpath(to);

  if (link(local_from, local_to) < 0) {
    retv = -errno;
  }

  free(local_from);
  free(local_to);
  return retv;
}

static int middfs_chmod(const char *path, mode_t mode,
			struct fuse_file_info *fi) {
  int retv = 0;
  char *local_path;

  local_path = middfs_localpath(path);

  if (chmod(local_path, mode) < 0) {
    retv = -errno;
  }

  free(local_path);
  return retv;
}

static int middfs_chown(const char *path, uid_t uid, gid_t gid,
			struct fuse_file_info *fi) {
  int retv = 0;
  char *localpath = NULL;

  localpath = middfs_localpath(path);

  if (lchown(localpath, uid, gid) < 0) {
    retv = -errno;
  }

  free(localpath);
  return retv;  
}

static int middfs_truncate(const char *path, off_t size,
			   struct fuse_file_info *fi) {
  int retv = 0;
  struct middfs_rsrc *rsrc = NULL;

  if (fi == NULL) {
    if ((rsrc = malloc(sizeof(*rsrc))) == NULL) {
      retv = -errno;
      goto cleanup;
    }
  } else {
    rsrc = (struct middfs_rsrc *) fi->fh;
  }

  switch (rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    retv = -EOPNOTSUPP;
    goto cleanup;
    
  case MR_LOCAL:
    if (rsrc->mr_fd >= 0) {
      if (ftruncate(rsrc->mr_fd, size) < 0) {
	retv = -errno;
	goto cleanup;
      }
    } else {
      char *localpath = middfs_localpath(path);
      if (truncate(localpath, size) < 0) {
	retv = -errno;
	goto cleanup;
      }
    }
    break;

  default:
    abort();
  }

 cleanup:
  if (fi == NULL) {
    int res = middfs_rsrc_delete(rsrc);
    if (retv >= 0) {
      retv = res;
    }
  }

  return retv;
}

static int middfs_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi) {
  int retv = 0;
  struct middfs_rsrc *rsrc = NULL;

  /* create resource */
  if ((rsrc = malloc(sizeof(*rsrc))) == NULL) {
    return -errno;
  }
  if ((retv = middfs_rsrc_create(path, rsrc)) < 0) {
    free(rsrc);
    return retv;
  }

  /* open resource */
  if ((retv = middfs_rsrc_open(rsrc, fi->flags, mode)) < 0) {
    middfs_rsrc_delete(rsrc);
    free(rsrc);
    return retv;
  }

  /* update file handle with resource */
  fi->fh = (uint64_t) rsrc;
  return retv;
}

static int middfs_open(const char *path, struct fuse_file_info *fi) {
  int retv = 0;

  /* create new resource */
  struct middfs_rsrc *rsrc;
  if ((rsrc = malloc(sizeof(*rsrc))) == NULL) {
    return -errno;
  }
  if ((retv = middfs_rsrc_create(path, rsrc)) < 0) {
    free(rsrc);
    return retv;
  }

  /* open resource */
  if ((retv = middfs_rsrc_open(rsrc, fi->flags)) < 0) {
    middfs_rsrc_delete(rsrc);
    free(rsrc);
    return retv;
  }

  /* update file handle with resource */
  fi->fh = (uint64_t) rsrc;
  return 0;
}

static int middfs_read(const char *path, char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi) {
  int retv = 0;
  struct middfs_rsrc *rsrc = NULL;
  struct middfs_rsrc rsrc_tmp;

  if (fi == NULL) {
    /* create & open temporary resource */
    if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
      return retv;
    }
    if ((retv = middfs_rsrc_open(&rsrc_tmp, O_RDONLY)) < 0) {
      goto cleanup;
    }
    /* set resource pointer */
    rsrc = &rsrc_tmp;
  } else {
    rsrc = (struct middfs_rsrc *) fi->fh;
  }

  if ((retv = pread(rsrc->mr_fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }

 cleanup:
  /* delete temporary resource if needed */
  if (fi == NULL) {
    int res = middfs_rsrc_delete(rsrc);
    retv = (retv < 0) ? retv : res;
  }

  return retv;
}

static int middfs_write(const char *path, const char *buf,
			size_t size, off_t offset,
			struct fuse_file_info *fi) {
  int retv = 0;
  struct middfs_rsrc *rsrc = NULL;
  struct middfs_rsrc rsrc_tmp;
  
  /* obtain resource */
  if (fi == NULL) {
    if ((retv = middfs_rsrc_create(path, &rsrc_tmp)) < 0) {
      return retv;
    }
    if ((retv = middfs_rsrc_open(&rsrc_tmp, O_WRONLY)) < 0) {
      goto cleanup;
    }
  } else {
    rsrc = (struct middfs_rsrc *) fi->fh;
  }

  if ((retv = pwrite(rsrc->mr_fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }

 cleanup:
  if (fi == NULL) {
    middfs_rsrc_delete(rsrc);
  }

  return retv;
}

static int middfs_statfs(const char *path, struct statvfs *stbuf) {
  int retv = 0;
  char *localpath;

  localpath = middfs_localpath(path);

  if (statvfs(localpath, stbuf) < 0) {
    retv = -errno;
  }

  free(localpath);
  return retv;
}

static int middfs_release(const char *path,
			  struct fuse_file_info *fi) {

  int retv = 0;
  struct middfs_rsrc *rsrc = (struct middfs_rsrc *) fi->fh;

  retv = middfs_rsrc_delete(rsrc);
  free(rsrc);

  return retv;
}

struct fuse_operations middfs_oper =
  {.init = middfs_init,
   .getattr = middfs_getattr,
   .access = middfs_access,
   .readlink = middfs_readlink,
#if 0   
   .mknod = middfs_mknod,
#endif
   .mkdir = middfs_mkdir,
   .symlink = middfs_symlink,
   .unlink = middfs_unlink,
   .rmdir = middfs_rmdir,
   .rename = middfs_rename,
   .link = middfs_link,
   .chmod = middfs_chmod,
   .chown = middfs_chown,
   .truncate = middfs_truncate,
   .open = middfs_open,
   .create = middfs_create,
   .read = middfs_read,
   .write = middfs_write,
   .statfs = middfs_statfs,
   .release = middfs_release,
   .readdir = middfs_readdir,
  };
