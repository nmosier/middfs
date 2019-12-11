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
#include <dirent.h>

#include "lib/middfs-conf.h"
#include "lib/middfs-pkt.h"
#include "lib/middfs-util.h"

#include "client/middfs-client-rsrc.h"
#include "client/middfs-client.h"
#include "client/middfs-client-conf.h"
#include "client/middfs-client-pkt.h"
#include "client/middfs-client-fuse.h"

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

/* client_rsrc_init() -- construct resource from path
 * ARGS:
 * RETV: returns 0 on success; -errno on error.
 */
int client_rsrc_init(const char *path, struct client_rsrc *client_rsrc) {
  /* find owner string in _path_ */
  const char *owner_begin = path + strspn(path, "/");

  if (owner_begin == path) {
    return -EINVAL; /* _path_ is relative (no leading '/') */
  }

  if (*owner_begin == '\0') {
    /* requesting middfs root -- special type of request */
     client_rsrc->mr_rsrc.mr_owner = strdup("\0");
     client_rsrc->mr_rsrc.mr_path = strdup("\0");
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

/* FILE I/O FUNCTIONS */

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
     {
        struct middfs_packet out = {0};
        struct middfs_packet in  = {0};
        packet_init(&out, MPKT_REQUEST);
        request_init(&out.mpkt_un.mpkt_request, MREQ_OPEN, &client_rsrc->mr_rsrc);
        out.mpkt_un.mpkt_request.mreq_mode = flags;
        if (packet_xchg(&out, &in) < 0) {
           perror("packet_xchg");
           return -EIO;
        }
        if ((retv = response_validate(&in, MRSP_OK)) < 0) {
           return retv;
        }

        return 0;
     }
     
  case MR_ROOT:
     return 0; /* TODO -- should send packet */
    
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
     {
        /* REAL CODE */
        struct middfs_packet out_pkt =
           {.mpkt_magic = MPKT_MAGIC,
            .mpkt_type = MPKT_REQUEST};
        struct middfs_packet in_pkt;

        /* init request */
        struct middfs_request *req = &out_pkt.mpkt_un.mpkt_request;
        req->mreq_type = MREQ_GETATTR;
        req->mreq_requester = conf_get(MIDDFS_CONF_USERNAME);
        req->mreq_rsrc = client_rsrc->mr_rsrc;

        /* exchange packets & validate response */
        if ((retv = packet_xchg(&out_pkt, &in_pkt)) < 0) {
           return retv;
        }
        if ((retv = response_validate(&in_pkt, MRSP_STAT)) < 0) {
           return retv;
        }
     
        /* set stat buf */
        const struct middfs_stat *mstat = &in_pkt.mpkt_un.mpkt_response.mrsp_un.mrsp_stat;
        sb->st_mode = mstat->mstat_mode;
        sb->st_size = mstat->mstat_size;
        sb->st_blocks = mstat->mstat_blocks;
        sb->st_blksize = mstat->mstat_blksize;

        /* populate other fields */
        sb->st_uid = getuid();
        sb->st_gid = getgid();
        sb->st_nlink = 1;
     }

     return 0;
     
  case MR_ROOT: /* stat info for middfs mountpoint */
     /* mark as directory, read-only for owner & group */
     sb->st_mode = S_IFDIR | S_IRUSR | S_IRGRP | S_IROTH;
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
     {
        struct middfs_packet out = {0};
        struct middfs_packet in = {0};
        packet_init(&out, MPKT_REQUEST);
        request_init(&out.mpkt_un.mpkt_request, MREQ_ACCESS, &client_rsrc->mr_rsrc);
        out.mpkt_un.mpkt_request.mreq_mode = mode;
        if (packet_xchg(&out, &in) < 0) {
           perror("packet_xchg");
           return -EIO;
        }
        if ((retv = response_validate(&in, MRSP_OK)) < 0) {
           return retv;
        }

        return 0;
     }
     
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
     {
        struct middfs_packet out = {0};
        struct middfs_packet in = {0};
        packet_init(&out, MPKT_REQUEST);
        struct middfs_request *req = &out.mpkt_un.mpkt_request;
        request_init(req, MREQ_TRUNCATE, &client_rsrc->mr_rsrc);
        req->mreq_size = size;
        if (packet_xchg(&out, &in) < 0) {
           perror("packet_xchg");
           return -EIO;
        }
        if ((retv = response_validate(&in, MRSP_OK)) < 0) {
           return retv;
        }
        return 0;
     }

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


int client_rsrc_readdir(struct client_rsrc *rsrc, void *buf, fuse_fill_dir_t filler,
                        off_t offset) {
   int retv = 0;

   switch (rsrc->mr_type) {
   case MR_NETWORK:
   case MR_ROOT:
      {
         /* construct a readdir request */
         struct middfs_packet in_pkt = {0};
         struct middfs_packet out_pkt = {0};
         packet_init(&out_pkt, MPKT_REQUEST);
         request_init(&out_pkt.mpkt_un.mpkt_request, MREQ_READDIR, &rsrc->mr_rsrc);

         if ((retv = packet_xchg(&out_pkt, &in_pkt)) < 0) {
            return retv;
         }

         /* validate response */
         if ((retv = response_validate(&in_pkt, MRSP_DIR)) < 0) {
            return retv;
         }

         const struct middfs_dir *dir = &in_pkt.mpkt_un.mpkt_response.mrsp_un.mrsp_dir;

         /* decode and fill buffer with dirents */
         for (uint64_t i = 0; i < dir->mdir_count; ++i) {
            const struct middfs_dirent *dirent = &dir->mdir_ents[i];
            struct stat st;
            memset(&st, 0, sizeof(st));
            st.st_mode = dirent->mde_mode;
            if (filler(
#if FUSE == 3	       
                       buf, dirent->mde_name, &st, 0, 0
#else
                       buf, dirent->mde_name, &st, 0
#endif
                       )) {
               break; /* buffer full */
            }
         }
         break;
      }

   case MR_LOCAL:
      {
         DIR *dir = NULL;
         struct dirent *dir_entry;
         /* open directory */
         if ((dir = fdopendir(rsrc->mr_fd)) == NULL) {
            return -errno;
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
            rsrc->mr_fd = -1; /* closedir deleted the fd */
         }

         break;
      }

   default:
      abort();
   }

   return retv;
}
