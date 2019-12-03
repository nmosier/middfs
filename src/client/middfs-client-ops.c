/* middfs-client-ops.c -- implementation of middfs-client file
 * operations
 * Tommaso Monaco & Nicholas Mosier, Oct 2019 
 */

#include "client/middfs-client-fuse.h"

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
#include <assert.h>

#include "lib/middfs-serial.h"
#include "lib/middfs-conn.h"
#include "lib/middfs-pkt.h"
#include "lib/middfs-util.h"
#include "lib/middfs-conf.h"

#include "client/middfs-client-ops.h"
#include "client/middfs-client-rsrc.h"
#include "client/middfs-client-handler.h"
#include "client/middfs-client-conf.h"

/* NOTE: return value is for private context and is passed
 * to all other middfs functions. Don't need to use for now,
 * though. */
#if FUSE == 3

static void *middfs_init(struct fuse_conn_info *conn,
			 struct fuse_config *cfg) {
  cfg->use_ino = 1; /* use inodes */
  
  /* disable caching */
  cfg->entry_timeout = 0.0;
  cfg->attr_timeout = 0.0;
  cfg->negative_timeout = 0.0;
  
  return NULL;  
}
#else /* FUSE == 2 */

static void *middfs_init(struct fuse_conn_info *conn) {
  return NULL;
}

#endif



static int middfs_getattr
(
#if FUSE == 3
 const char *path, struct stat *stbuf, struct fuse_file_info *fi
#else
 const char *path, struct stat *stbuf
#endif
)
{
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;

#if FUSE == 2
  void *fi = NULL; /* fi parameter is missing in FUSE 2 API */
#endif

  if (fi == NULL) {
    /* create & open temporary resource */
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  }
#if FUSE == 3
  else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }
#endif

  /* get lstat info */
  if ((retv = client_rsrc_lstat(client_rsrc, stbuf)) < 0) {
    goto cleanup;
  }

 cleanup:
  /* delete temporary resource if needed */
  if (fi == NULL) {
    int res = client_rsrc_delete(&client_rsrc_tmp);
    retv = (retv < 0) ? retv : res; /* correctly set return val */
  }
  
  return retv;
}

static int middfs_access(const char *path, int mode) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;

  /* create temporary resource */
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }

  /* check access */
  retv = client_rsrc_access(&client_rsrc_tmp, mode);

  /* cleanup */
  res = client_rsrc_delete(&client_rsrc_tmp);
  retv = (retv < 0) ? retv : res;

  return retv;
}

static int middfs_readlink(const char *path, char *buf,
			   size_t size) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;
  
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }
  if ((retv = client_rsrc_readlink(&client_rsrc_tmp, buf, size)) < 0) {
    goto cleanup;
  }

  buf[retv] = '\0';
  
 cleanup:
  res = client_rsrc_delete(&client_rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  return retv;
}

static int middfs_readdir
(
#if FUSE == 3
 const char *path, void *buf,
 fuse_fill_dir_t filler, off_t offset,
 struct fuse_file_info *fi,
 enum fuse_readdir_flags flags
#else
 const char *path, void *buf,
 fuse_fill_dir_t filler, off_t offset,
 struct fuse_file_info *fi
#endif
)
{
  DIR *dir = NULL;
  struct dirent *dir_entry;
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;

  /* DEBUGGING */
  fprintf(stderr, "``%s''", conf_get(MIDDFS_CONF_HOMEPATH));
  
  /* get resource handle */
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }
  if ((retv = client_rsrc_open(&client_rsrc_tmp, O_RDONLY)) < 0) {
    goto cleanup;
  }
  
  /* open directory */
  if ((dir = fdopendir(client_rsrc_tmp.mr_fd)) == NULL) {
    retv = -errno;
    goto cleanup;
  }
  
  while ((dir_entry = readdir(dir)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_ino = dir_entry->d_ino;
    st.st_mode = dir_entry->d_type << 12; /* no clue... */
    
    if (filler(
#if FUSE == 3	       
	       buf, dir_entry->d_name, &st, 0, 0
#else
	       buf, dir_entry->d_name, &st, 0
#endif
	       )) {
      break; /* buffer full */
    }
  }
  
  if (closedir(dir) < 0) {
    retv = -errno;
  } else {
    client_rsrc_tmp.mr_fd = -1; /* closedir deleted the fd */
  }

 cleanup:
  if ((res = client_rsrc_delete(&client_rsrc_tmp)) < 0) {
    if (retv >= 0) {
      retv = res;
    }
  }

  
  return retv;
}


static int middfs_mkdir(const char *path, mode_t mode) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;

  
  /* acquire temporary resource */
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }

  /* mkdir for resource */
  retv = client_rsrc_mkdir(&client_rsrc_tmp, mode);

  /* cleanup */
  res = client_rsrc_delete(&client_rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  
  return retv;
}

static int middfs_unlink(const char *path) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;
  
  /* acquire temporary resource */
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }
  retv = client_rsrc_unlink(&client_rsrc_tmp);

  res = client_rsrc_delete(&client_rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  
  return retv;
}

static int middfs_rmdir(const char *path) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;
  
  if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
    return retv;
  }
  retv = client_rsrc_rmdir(&client_rsrc_tmp);

  res = client_rsrc_delete(&client_rsrc_tmp);
  retv = (retv < 0) ? retv : res;
  
  return retv;
}

static int middfs_symlink(const char *to, const char *from) {
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;

  if ((retv = client_rsrc_create(from, &client_rsrc_tmp)) < 0) {
    return retv;
  }

  retv = client_rsrc_symlink(&client_rsrc_tmp, to);

  res = client_rsrc_delete(&client_rsrc_tmp);
  if (retv == 0) {
     retv = res;
  }

  return retv;
}

static int middfs_rename
(
#if FUSE == 3
 const char *from, const char *to, unsigned int flags
#else
 const char *from, const char *to
#endif
 )
{
  int retv = 0;
  struct client_rsrc client_rsrc_from, client_rsrc_to;

  /* create resources */
  if ((retv = client_rsrc_create(from, &client_rsrc_from)) < 0) {
    return retv;
  }
  if ((retv = client_rsrc_create(to, &client_rsrc_to)) < 0) {
    goto cleanup;
  }

  /* rename */
  retv = client_rsrc_rename(&client_rsrc_from, &client_rsrc_to);

 cleanup:
  client_rsrc_delete(&client_rsrc_from);
  client_rsrc_delete(&client_rsrc_to); // TODO: error checking.

  return retv;
  
}

static int middfs_link(const char *from, const char *to) {
   abort();
}

static int middfs_chmod
(
#if FUSE == 3
 const char *path, mode_t mode, struct fuse_file_info *fi
#else
 const char *path, mode_t mode
#endif
 )
{
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;

#if FUSE == 2
  void *fi = NULL; /* fi parameter missing in FUSE 2 API */
#endif

  if (fi == NULL) {
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  }
#if FUSE == 3
  else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }
#endif

  retv = client_rsrc_chmod(client_rsrc, mode);

  /* cleanup */
  if (fi == NULL) {
    client_rsrc_delete(&client_rsrc_tmp);
  }
  
  return retv;
}

static int middfs_chown(const char *path, uid_t uid, gid_t gid,
			struct fuse_file_info *fi) {
   abort();
}

static int middfs_truncate
(
#if FUSE == 3
 const char *path, off_t size, struct fuse_file_info *fi
#else
 const char *path, off_t size
#endif
 )
{
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;

#if FUSE == 2
  void *fi = NULL; /* fi parameter missing in FUSE 2 API */
#endif
  
  if (fi == NULL) {
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  }
#if FUSE == 3
  else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }
#endif

  retv = client_rsrc_truncate(client_rsrc, size);

  if (fi == NULL) {
    int res = client_rsrc_delete(&client_rsrc_tmp);
    if (retv >= 0) {
      retv = res;
    }
  }

  return retv;
}

static int middfs_create(const char *path, mode_t mode,
			 struct fuse_file_info *fi) {
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;

  /* create resource */
  if ((client_rsrc = malloc(sizeof(*client_rsrc))) == NULL) {
    return -errno;
  }
  if ((retv = client_rsrc_create(path, client_rsrc)) < 0) {
    free(client_rsrc);
    return retv;
  }

  /* open resource */
  if ((retv = client_rsrc_open(client_rsrc, fi->flags, mode)) < 0) {
    client_rsrc_delete(client_rsrc);
    free(client_rsrc);
    return retv;
  }

  /* update file handle with resource */
  fi->fh = (uint64_t) client_rsrc;
  return retv;
}

static int middfs_open(const char *path, struct fuse_file_info *fi) {
  int retv = 0;

  /* create new resource */
  struct client_rsrc *client_rsrc;
  if ((client_rsrc = malloc(sizeof(*client_rsrc))) == NULL) {
    return -errno;
  }
  if ((retv = client_rsrc_create(path, client_rsrc)) < 0) {
    free(client_rsrc);
    return retv;
  }
  
  /* open resource */
  if ((retv = client_rsrc_open(client_rsrc, fi->flags)) < 0) {
    client_rsrc_delete(client_rsrc);
    free(client_rsrc);
    return retv;
  }
  
  /* update file handle with resource */
  fi->fh = (uint64_t) client_rsrc;
  return 0;
}


#define BUFSIZE 256
static int middfs_read(const char *path, char *buf, size_t size,
		       off_t offset, struct fuse_file_info *fi) {
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;

  if (fi == NULL) {
    /* create & open temporary resource */
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    if ((retv = client_rsrc_open(&client_rsrc_tmp, O_RDONLY)) < 0) {
      goto cleanup;
    }
    /* set resource pointer */
    client_rsrc = &client_rsrc_tmp;
  } else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }

  if ((retv = client_rsrc_read(client_rsrc, buf, size, offset)) < 0) {
     errno = -retv;
     perror("client_rsrc_read");
  }
  
 cleanup:
  /* delete temporary resource if needed */
  if (fi == NULL) {
    int res = client_rsrc_delete(client_rsrc);
    retv = (retv < 0) ? retv : res;
  }

  return retv;
}

static int middfs_write(const char *path, const char *buf,
			size_t size, off_t offset,
			struct fuse_file_info *fi) {
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;
  
  /* obtain resource */
  if (fi == NULL) {
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    if ((retv = client_rsrc_open(&client_rsrc_tmp, O_WRONLY)) < 0) {
      goto cleanup;
    }
  } else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }

#if 0
  if ((retv = pwrite(client_rsrc->mr_fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }
#else
  if ((retv = client_rsrc_write(client_rsrc, buf, size, offset)) < 0) {
     errno = -retv;
     perror("client_rsrc_write");
  }
#endif

 cleanup:
  if (fi == NULL) {
    client_rsrc_delete(client_rsrc);
  }

  return retv;
}

static int middfs_statfs(const char *path, struct statvfs *stbuf) {
  int retv = 0;

  if (statvfs(path, stbuf) < 0) {
    retv = -errno;
  }

  return retv;
}

static int middfs_release(const char *path,
			  struct fuse_file_info *fi) {

  int retv = 0;
  struct client_rsrc *client_rsrc = (struct client_rsrc *) fi->fh;

  retv = client_rsrc_delete(client_rsrc);
  free(client_rsrc);

  errno = ENOENT; /* TEST */
  
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
   .link = middfs_link, /* not supported */
   .chmod = middfs_chmod,
   .chown = middfs_chown, /* not supported */
   .truncate = middfs_truncate,
   .open = middfs_open,
   .create = middfs_create,
   .read = middfs_read,
   .write = middfs_write,
   .statfs = middfs_statfs,
   .release = middfs_release,
   .readdir = middfs_readdir,
  };
