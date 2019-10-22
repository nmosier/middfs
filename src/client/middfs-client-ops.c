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
#include <assert.h>

// TEMPORARY //
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/mman.h>
// TEMPORARY //

#include "middfs-client-ops.h"
#include "middfs-client-rsrc.h"
#include "middfs-serial.h"

#define LISTEN_PORT_DEFAULT 56789

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
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;
    
  if (fi == NULL) {
    /* create & open temporary resource */
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  } else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }

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

static int middfs_readdir(const char *path, void *buf,
			  fuse_fill_dir_t filler, off_t offset,
			  struct fuse_file_info *fi,
			  enum fuse_readdir_flags flags) {
  DIR *dir = NULL;
  struct dirent *dir_entry;
  int retv = 0;
  int res;
  struct client_rsrc client_rsrc_tmp;

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
    memset(&st, sizeof(st), 0);
    st.st_ino = dir_entry->d_ino;
    st.st_mode = dir_entry->d_type << 12; /* no clue... */
    if (filler(buf, dir_entry->d_name, &st, 0, 0)) {
      break; /* buffer full? */
    }
  }

  if (fdclosedir(dir) < 0) {
    retv = -errno;
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
  retv < 0 || (retv = res);

  return retv;
}

static int middfs_rename(const char *from, const char *to,
			 unsigned int flags) {
  int retv = 0;
  struct client_rsrc client_rsrc_from, client_rsrc_to;

  /* no flags are currently supported */
  if (flags) {
    return -EINVAL;
  }

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

#if 0 // No hardlinks supported yet
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
#endif

static int middfs_chmod(const char *path, mode_t mode,
			struct fuse_file_info *fi) {
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;
  
  if (fi != NULL) {
    client_rsrc = (struct client_rsrc *) fi->fh;
  } else {
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  }

  retv = client_rsrc_chmod(client_rsrc, mode);

  /* cleanup */
  if (fi == NULL) {
    client_rsrc_delete(&client_rsrc_tmp);
  }
  
  return retv;
}

#if 0 /* no changing ownership of local files for now */
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
#endif

static int middfs_truncate(const char *path, off_t size,
			   struct fuse_file_info *fi) {
  int retv = 0;
  struct client_rsrc *client_rsrc = NULL;
  struct client_rsrc client_rsrc_tmp;

  if (fi == NULL) {
    if ((retv = client_rsrc_create(path, &client_rsrc_tmp)) < 0) {
      return retv;
    }
    client_rsrc = &client_rsrc_tmp;
  } else {
    client_rsrc = (struct client_rsrc *) fi->fh;
  }

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

  // TEMPORARY
  struct rsrc *rsrc = &client_rsrc->mr_rsrc;
  int sockfd;
  if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("socket");
  }
  int servport = LISTEN_PORT_DEFAULT;
  struct sockaddr_in addr = {0};
  addr.sin_addr.s_addr = inet_addr("140.233.20.6");
  addr.sin_family = AF_INET;
  addr.sin_port = htons(servport);

  int conn;
  if ((conn = connect(sockfd, (struct sockaddr *) &addr,
		      sizeof(addr))) < 0) {
    perror("connect");
  }

#if 1

  // stupid test to produce "preliminary results"
  size_t nbytes = 0x100;
  FILE *outf = fopen("/home/nicholas/middfs/client.out", "w");
  int infd = open("/home/nicholas/middfs/file.in", O_RDONLY);
  uint8_t *data = mmap(NULL, nbytes, PROT_READ, 0, infd, 0);

  assert(outf != NULL && infd >= 0 && data != MAP_FAILED);
  
  //  for (int i = 0; i < 16; ++i) {
  clock_t clk_begin, clk_end;
  size_t bytes_sent = 0;
  clk_begin = clock();
  while ((bytes_sent += write(sockfd, data, nbytes - bytes_sent))
	 < nbytes) {}
  clk_end = clock();
  fprintf(outf, "%zx: %d\n", bytes_sent, clk_end - clk_begin);
    //    nbytes *= 16;
    //  }

  clk_begin = clock();
  sleep(1);
  clk_end = clock();
  fprintf(outf, "clock reference: 1 sec = %d\n",
	  clk_end - clk_begin);
  fclose(outf);
  munmap(data, nbytes);
  close(infd);
  
#else
  char buf[64];
  size_t bytes;
  if ((bytes = serialize_rsrc(rsrc, buf, 64)) > 64) {
    fprintf(stderr, "too many bytes\n");
  }

  if (send(sockfd, buf, 1, 0) < 1) {
    perror("send");
  }
  sleep(1); /* wait 1 second */
  if (send(sockfd, buf + 1, bytes - 1, 0) < bytes - 1) {
    perror("send");
  }

#endif
 
  close(sockfd);
  
  
  // TEMPORARY

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

  if ((retv = pread(client_rsrc->mr_fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
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

  if ((retv = pwrite(client_rsrc->mr_fd, buf, size, offset)) < 0) {
    retv = -errno;
    goto cleanup;
  }

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
#if 0   
   .link = middfs_link,
#endif   
   .chmod = middfs_chmod,
#if 0
   .chown = middfs_chown,
#endif
   .truncate = middfs_truncate,
   .open = middfs_open,
   .create = middfs_create,
   .read = middfs_read,
   .write = middfs_write,
   .statfs = middfs_statfs,
   .release = middfs_release,
   .readdir = middfs_readdir,
  };
