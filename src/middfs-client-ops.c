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
#include "middfs-client-util.h"

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
  
  /* obtain actual path through mirror */
  char *localpath = middfs_localpath(path);

  if (lstat(localpath, stbuf) < 0) {
    retv = -errno;
  }

  /* cleanup */
  free(localpath);
  return retv;
}

static int middfs_access(const char *path, int mode) {
  int retv = 0;

  char *localpath = middfs_localpath(path);
  
  if (access(localpath, mode) < 0) {
    retv = -errno;
  }

  /* cleanup */
  free(localpath);
  return retv;
}

static int middfs_readlink(const char *path, char *buf,
			   size_t size) {
  ssize_t bytes;
  int retv = 0;
  char *localpath = middfs_localpath(path);

  if ((bytes = readlink(path, buf, size - 1)) < 0) {
    retv = -errno;
  } else {
    buf[bytes] = '\0';
  }

  /* cleanup */
  free(localpath);
  return retv;
}

static int middfs_readdir(const char *path, void *buf,
			  fuse_fill_dir_t filler, off_t offset,
			  struct fuse_file_info *fi,
			  enum fuse_readdir_flags flags) {
  DIR *dir;
  struct dirent *dir_entry;
  char *localpath;
  int retv = 0;
  
  localpath = middfs_localpath(path);
  
  if ((dir = opendir(localpath)) == NULL) {
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
  if (dir != NULL && closedir(dir) < 0 && retv != 0) {
      retv = -errno;
  }
  free(localpath);

  return retv;
}


static int middfs_mknod(const char *path, mode_t mode, dev_t rdev) {
  int retv = 0;
  char *localpath = middfs_localpath(path);

#if 0
  if (mknod_wrapper(AT_FDCWD, localpath, NULL,mode, rdev) < 0) {
    retv = -errno;
  }
#endif

  free(localpath);
  return retv;
}


static int middfs_mkdir(const char *path, mode_t mode) {
  int retv = 0;
  char *localpath = middfs_localpath(path);

  if (mkdir(localpath, mode) < 0) {
    retv = -errno;
  }
  
  free(localpath);
  return retv;
}

static int middfs_unlink(const char *path) {
  int retv = 0;
  char *localpath = middfs_localpath(path);

  if (unlink(localpath) < 0) {
    retv = -errno;
  }

  free(localpath);
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
  char *localpath;

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

  /* if file info has a file descriptor, use that;
   * otherwise, truncate file using path */
  if (fi != NULL) {
    retv = ftruncate(fi->fh, size);
  } else {
    char *local_path = middfs_localpath(path);
    retv = truncate(local_path, size);
    free(local_path);
  }

  return retv < 0 ? -errno : 0;
}

static int middfs_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi) {
  int retv = 0;
  char *local_path;
  int fd;

  local_path = middfs_localpath(path);

  /* open file (in mirror) */
  if ((fd = open(local_path, fi->flags, mode)) < 0) {
    retv = -errno;
    goto cleanup;
  }

  /* update file handle */
  fi->fh = fd;

 cleanup:
  free(local_path);
  return retv;
}

static int middfs_open(const char *path, struct fuse_file_info *fi) {
  int retv = 0;
  int fd;
  char *localpath;

  localpath = middfs_localpath(path);

  /* open file; obtain file descriptor */
  if ((fd = open(localpath, fi->flags)) < 0) {
    retv = -errno;
    goto cleanup;
  }

  /* update file handle */
  fi->fh = fd;
  
 cleanup:
  free(localpath);
  return retv;
}

static int middfs_read(const char *path, char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi) {
  int fd;
  int retv;
  char *localpath;

  localpath = middfs_localpath(path);

  /* TODO: figure out why all this is necessary (if it is). */
  if (fi == NULL) {
    if ((fd = open(localpath, O_RDONLY)) < 0) {
      retv = -errno;
      goto cleanup;
    }
  } else {
    fd = fi->fh;
  }

  /* read bytes */
  if ((retv = pread(fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }

 cleanup:
  if (fd >= 0 && fi == NULL && close(fd) < 0 && retv >= 0) {
    retv = -errno;
  }
  free(localpath);
  return retv;
}

static int middfs_write(const char *path, const char *buf,
			size_t size, off_t offset,
			struct fuse_file_info *fi) {
  int fd;
  int retv;
  char *localpath;

  localpath = middfs_localpath(path);

  /* get writable file descriptor */
  if (fi == NULL) {
    if ((fd = open(localpath, O_WRONLY)) < 0) {
      retv = -errno;
      goto cleanup;
    }
  } else {
    fd = fi->fh;
  }

  /* write data */
  if ((retv = pwrite(fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }

 cleanup:
  if (fi == NULL && fd >= 0 && close(fd) < 0 && retv >= 0) {
    retv = -errno;
  }
  free(localpath);
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
  return close(fi->fh) < 0 ? -errno : 0;
}

struct fuse_operations middfs_oper =
  {.init = middfs_init,
   .getattr = middfs_getattr,
   .access = middfs_access,
   .readlink = middfs_readlink,
   .mknod = middfs_mknod,
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
