/* middfs-client-client_rsrc.c -- implementation of utility functions for
 * middfs-client
 * Tommaso Monaco & Nicholas Mosier
 * Oct 2019
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#include "lib/middfs-conf.h"
#include "lib/middfs-pkt.h"
#include "lib/middfs-util.h"

#include "client/middfs-client-rsrc.h"
#include "client/middfs-client.h"
#include "client/middfs-client-conf.h"
#include "client/middfs-client-pkt.h"

/* DEBUGGING */
#define STAT_EXAMPLE_REG "/write"
#define STAT_EXAMPLE_DIR "/dir"

/* utility function definitions */

char *middfs_localpath_tmp(const char *middfs_path) {
  char *localpath;
  asprintf(&localpath, "%s/%s%s", conf_get(MIDDFS_CONF_HOMEPATH),
           conf_get(MIDDFS_CONF_USERNAME), middfs_path);
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


/* client_rsrc_* FUNCTIONS
 * NOTE: All client_rsrc_* functions return negated error codes directly
 * (e.g. return -errno).
 */

/* client_rsrc_create() -- construct resource from path
 * ARGS:
 * RETV: returns 0 on success; -errno on error.
 */
int client_rsrc_create(const char *path, struct client_rsrc *client_rsrc) {
  /* find owner string in _path_ */
  const char *owner_begin = path + strspn(path, "/");

  if (owner_begin == path) {
    return -EINVAL; /* _path_ is relative (no leading '/') */
  }

  if (*owner_begin == '\0') {
    /* requesting middfs root -- special type of request */
    client_rsrc->mr_rsrc.mr_owner = client_rsrc->mr_rsrc.mr_path = NULL;
    client_rsrc->mr_type = MR_ROOT;
  } else {
    /* requesting something in a user directory */
    
    const char *owner_end = owner_begin + strcspn(owner_begin, "/");
  
    /* dup and save owner string */
    client_rsrc->mr_rsrc.mr_owner = strndup(owner_begin, owner_end - owner_begin);
  
    /* find owner path string */
    if (*owner_end == '\0') {
      client_rsrc->mr_rsrc.mr_path = strdup("/");
    } else {
      client_rsrc->mr_rsrc.mr_path = strdup(owner_end);
    }

    /* find resource type */
    if (strcmp(client_rsrc->mr_rsrc.mr_owner, conf_get(MIDDFS_CONF_USERNAME)) != 0) {
      client_rsrc->mr_type = MR_NETWORK; /* resource not owned by client */
    } else {
      client_rsrc->mr_type = MR_LOCAL; /* resource owned by client */
    }

  }
  
  /* initialize other fields */
  client_rsrc->mr_fd = -1;
  
  return 0; /* success */
}

int client_rsrc_delete(struct client_rsrc *client_rsrc) {
  int retv = 0;

  if (client_rsrc != NULL) {
    if (client_rsrc->mr_fd >= 0) {
      if (close(client_rsrc->mr_fd) < 0) {
	retv = -errno;
      }
    }
    
    free(client_rsrc->mr_rsrc.mr_owner);
    free(client_rsrc->mr_rsrc.mr_path);
  }
  
  return retv;
}

int client_rsrc_open(struct client_rsrc *client_rsrc, int flags, ...) {
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
  
  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
     /* TODO: This is a purely temporary measure. 
      * Write some REAL code next time. */
     return 0;
    
  case MR_LOCAL:
    /* open local file */
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if ((fd = open(localpath, flags, mode)) < 0) {
      retv = -errno;
    } else {
      client_rsrc->mr_fd = fd;
    }
    free(localpath);
    return retv;
    
  default:
    abort();
  }
}

int client_rsrc_lstat(const struct client_rsrc *client_rsrc,
		      struct stat *sb) {
  int retv = 0;
  char *localpath = NULL;

  memset(sb, 0, sizeof(*sb)); /* initialize buffer */

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
#if 1
     /* TODO: This is slapdash code. Fix later. */
     /* check if path is dir or reg file */
     if (strcmp(client_rsrc->mr_rsrc.mr_path, "/") != 0) {
        /* is a regular file */
        sb->st_mode = S_IFREG | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IWOTH;
     } else {
        /* is a directory */
        sb->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IWOTH;
     }
     sb->st_uid = getuid();
     sb->st_gid = getgid();
     sb->st_size = 64; // 1e10; /* TODO: ... */
     sb->st_blksize = 4096;
     sb->st_blocks = 1;
     sb->st_nlink = 1;
#else
     /* REAL CODE */
     


#endif

     
     return 0;
     
  case MR_ROOT: /* stat info for middfs mountpoint */
     
     /* mark as directory, read-only for owner & group */
     sb->st_mode = S_IFDIR | S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH | S_IWOTH;              /* fis ti*/
     sb->st_uid = getuid(); /* owner is owner of this FUSE process */
     sb->st_gid = getgid(); /* group is group of this FUSE process */
     return 0;
     
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if (lstat(localpath, sb) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int client_rsrc_readlink(const struct client_rsrc *client_rsrc,
                         char *buf, size_t bufsize) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if ((retv = readlink(localpath, buf, bufsize)) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int client_rsrc_access(const struct client_rsrc *client_rsrc, int mode) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
     return 0;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if (access(localpath, mode) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int client_rsrc_mkdir(const struct client_rsrc *client_rsrc, mode_t mode) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if (mkdir(localpath, mode) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int client_rsrc_unlink(const struct client_rsrc *client_rsrc) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
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

int client_rsrc_rmdir(const struct client_rsrc *client_rsrc) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if (rmdir(localpath) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;
    
  default:
    abort();
  }
}


int client_rsrc_truncate(const struct client_rsrc *client_rsrc, off_t size) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    if (client_rsrc->mr_fd >= 0) {
      if (ftruncate(client_rsrc->mr_fd, size) < 0) {
	retv = -errno;
      }
    } else {
      localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
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

int client_rsrc_symlink(const struct client_rsrc *client_rsrc,
			const char *to) {
  int retv = 0;
  char *localpath = NULL;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;

  case MR_LOCAL:
    localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
    if (symlink(to, localpath) < 0) {
      retv = -errno;
    }
    free(localpath);
    return retv;

  default:
    abort();
  }
}

int client_rsrc_rename(const struct client_rsrc *from,
		       const struct client_rsrc *to) {
  int retv = 0;
  
  if (from->mr_type == MR_LOCAL && to->mr_type == MR_LOCAL) {
    char *local_to, *local_from;
    local_from = middfs_localpath_tmp(from->mr_rsrc.mr_path);
    local_to = middfs_localpath_tmp(to->mr_rsrc.mr_path);

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

int client_rsrc_chmod(const struct client_rsrc *client_rsrc, mode_t mode) {
  int retv = 0;
  char *localpath;

  switch (client_rsrc->mr_type) {
  case MR_NETWORK:
  case MR_ROOT:
    return -EOPNOTSUPP;
    
  case MR_LOCAL:
    if (client_rsrc->mr_fd >= 0) {
      if (fchmod(client_rsrc->mr_fd, mode) < 0) {
	retv = -errno;
      }
    } else {
      localpath = middfs_localpath_tmp(client_rsrc->mr_rsrc.mr_path);
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

int client_rsrc_read(const struct client_rsrc *client_rsrc, char *buf, size_t size, off_t offset) {
   int retv = 0;

   switch (client_rsrc->mr_type) {
   case MR_NETWORK:
      {
         /* construct packet */
         struct middfs_packet out_pkt =
            {.mpkt_magic = MPKT_MAGIC,
             .mpkt_type = MPKT_REQUEST,
             .mpkt_un = {.mpkt_request = {.mreq_type = MREQ_READ,
                                          .mreq_requester = strdup(conf_get(MIDDFS_CONF_USERNAME)),
                                          .mreq_rsrc = client_rsrc->mr_rsrc,
                                          .mreq_size = size,
                                          .mreq_off = offset
                                          }
                         }
            };
         struct middfs_packet in_pkt = {0};

         if ((retv = packet_xchg(&out_pkt, &in_pkt)) < 0) {
            return retv;
         }

         /* validate response */
         if (in_pkt.mpkt_magic != MPKT_MAGIC || in_pkt.mpkt_type != MPKT_RESPONSE) {
            return -EIO;
         }
         
         const struct middfs_response *rsp = &in_pkt.mpkt_un.mpkt_response;
         switch (rsp->mrsp_type) {
         case MRSP_DATA:
            {
               /* copy data from response */
               const struct middfs_data *data = &in_pkt.mpkt_un.mpkt_response.mrsp_un.mrsp_data;
               size_t nbytes = MIN(size, data->mdata_nbytes);
               memcpy(buf, data->mdata_buf, nbytes);
               retv = nbytes;
               return retv;
            }
            
         case MRSP_ERROR:
            /* return specific error */
            return -rsp->mrsp_un.mrsp_error;
            
         default:
            return -EIO; /* packet corrupted */
         }
      }
         
   case MR_ROOT:
      return -EOPNOTSUPP;
      
   case MR_LOCAL:
      if ((retv = pread(client_rsrc->mr_fd, buf, size, offset)) < 0) {
         retv = -errno;
      }
      return retv;

   default:
      abort();
   }
}

int client_rsrc_write(const struct client_rsrc *client_rsrc, const void *buf,
                      size_t size, off_t offset) {
   int retv = 0;

   switch (client_rsrc->mr_type) {
   case MR_NETWORK:
      {
         /* construct packet */
         char *username = conf_get(MIDDFS_CONF_USERNAME);
         char *username_dup;
         void *buf_dup;
         
         assert(username && *username);
         username_dup = strdup(username);
         buf_dup = memdup(buf, size);

         if (buf_dup == NULL || username_dup == NULL) {
            free(buf_dup);
            free(username_dup);
            return -errno;
         }
         
         struct middfs_packet out_pkt =
            {.mpkt_magic = MPKT_MAGIC,
             .mpkt_type = MPKT_REQUEST,
             .mpkt_un = {.mpkt_request = {.mreq_type = MREQ_WRITE,
                                          .mreq_requester = username_dup,
                                          .mreq_rsrc = client_rsrc->mr_rsrc,
                                          .mreq_size = size,
                                          .mreq_off = offset,
                                          .mreq_data = buf_dup
                                          }
                         }
            };
         struct middfs_packet in_pkt;

         if (packet_xchg(&out_pkt, &in_pkt) < 0) {
            return -1;
         }

         /* validate response */
         assert(in_pkt.mpkt_magic == MPKT_MAGIC);
         assert(in_pkt.mpkt_type == MPKT_RESPONSE);
         assert(in_pkt.mpkt_un.mpkt_response.mrsp_type == MRSP_OK);
         
         return (retv < 0) ? retv : size;
      }
      
   case MR_ROOT:
      return -EOPNOTSUPP;
      
   case MR_LOCAL:
      if ((retv = pwrite(client_rsrc->mr_fd, buf, size, offset)) < 0) {
         retv = -errno;
      }
      return retv;
      
   default:
      abort();
   }
}

/***********************************
 *    Network-Related Functions    *
 ***********************************/

