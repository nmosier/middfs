/* middfs client file system
 * Tommaso Monaco & Nicholas Mosier
 * Sep 2019
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



/* defaults */
/* (NOT USED) */
#if 0
#define MIDDFS_CONF_DIR ".middfs-d"
#define MIDDFS_CONF_MIRROR "mirror"
#define MIDDFS_CONF_FILE "middfs.conf"
#endif

#define OPTDEF(t, p) {t, offsetof(struct middfs_opts, p), 1}

/* middfs options 
 * NOTE: Must be global because FUSE API doesn't know about this.
 */

struct middfs_opts {
  int show_help;
  char *root_dir;
  char *mirror_dir;
  char *client_dir;
};

struct middfs_conf {
  char *local_dir; /* logical client dir -- can be mirror_dir or client_dir */
};

/* GLOBALS */

static const struct fuse_opt option_spec[] =
  {/* Required Parameters */
   OPTDEF("--root=%s", root_dir),
   OPTDEF("--mirror=%s", mirror_dir),
   OPTDEF("--dir=%s", client_dir),
   /* Optional Parameters */
   OPTDEF("--help", show_help),   
   FUSE_OPT_END
  };

static struct middfs_opts middfs_opts;
static struct middfs_conf middfs_conf;

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

#if 0
  if (lstat(localpath, stbuf) < 0) {
    retv = -errno;
  }
#else
  while (lstat(localpath, stbuf) < 0) {
    int eintr = EINTR;
    if (errno != eintr) {
      retv = -errno;
      break;
    }
  }
#endif

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
  int retv;
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

static struct fuse_operations middfs_oper =
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

void show_help(const char *arg0) {
  fprintf(stderr,
	  "usage: %s [option...] dir\n"					\
	  "Options:\n"							\
	  "  --root=<dir>    middfs directory\n"			\
	  "  --mirror=<dir>  directory to mount mirror of root dir\n"	\
	  "  --dir=<dir>     client directory\n"			\
	  "  --help          display this help dialog\n",
	  arg0);
}


void middfs_defaultopts(struct middfs_opts *opts) {
  memset(opts, sizeof(*opts, 0), 0);
}

int middfs_mirror_mount(const char *dir, const char *mirror) {
  char *cmd;
  int retv = 0;
  asprintf(&cmd, "mount_nullfs %s %s", dir, mirror);
  fprintf(stderr, "%s\n", cmd); /* log command before execution (like Makefiles) */
  if (system(cmd) != 0) {
    fprintf(stderr, "middfs: error mounting mirror to %s at %s\n",
	    dir, mirror);
    retv = -1;
  }
  
  free(cmd);
  return retv;
}

int middfs_mirror_unmount(const char *mirror) {
  char *cmd;
  int retv= 0;
  asprintf(&cmd, "umount %s", mirror);
  fprintf(stderr, "%s\n", cmd);
  if (system(cmd) != 0) {
    fprintf(stderr, "middfs: error unmounting mirror at %s\n", mirror);
    retv = -1;
  }

  free(cmd);
  return retv;
}

int main(int argc, char *argv[]) {
  int retv = 1;
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
  int should_mirror, mounted_mirror;

  should_mirror = mounted_mirror = 0;
  
  /* set middfs option defaults */
  middfs_defaultopts(&middfs_opts);
  memset(&middfs_conf, sizeof(middfs_conf), 0);
  
  /* parse options */
  if (fuse_opt_parse(&args, &middfs_opts,
		     option_spec, NULL) < 0) {
    return 1; /* exit */
  }

  if (middfs_opts.show_help) {
    if (fuse_opt_add_arg(&args, "--help") == 0) {
      show_help(argv[0]);
      goto cleanup;
    }
    return 0;
  }

  /* validate options */
  int opt_valid = 1;
  if (middfs_opts.root_dir == NULL) {
    fprintf(stderr, "%s: required: --root=<path>\n", argv[0]);
    opt_valid = 0;
  } else {
    /* convert to absolute path */
    if ((errno = -middfs_abspath(&middfs_opts.root_dir)) > 0) {
      perror("middfs_abspath");
      goto cleanup;
    }
  }

  /* NOTE: mirror dir not required. */
  if (middfs_opts.mirror_dir != NULL) {
    should_mirror = 1;
    
    /* convert to absolute path */
    if ((errno = -middfs_abspath(&middfs_opts.mirror_dir)) > 0) {
      perror("middfs_abspath");
      goto cleanup;
    }
  }

  if (middfs_opts.client_dir == NULL) {
    fprintf(stderr, "%s: required: --dir=<path>\n", argv[0]);
    opt_valid = 0;
  } else {
    /* convert to absolute path */
    if ((errno = -middfs_abspath(&middfs_opts.client_dir)) > 0) {
      perror("middfs_abspath");
      goto cleanup;
    }
  }
  
  if (!opt_valid) {
    goto cleanup;
  }

  /* set local user directory access path */
  if (should_mirror) {
    middfs_conf.local_dir = middfs_opts.mirror_dir;
  } else {
    middfs_conf.local_dir = middfs_opts.client_dir;
  }
  
  /* set up mirror before FUSE mounts */
  if (should_mirror) {
    if (middfs_mirror_mount(middfs_opts.client_dir,
			    middfs_opts.mirror_dir) < 0) {
      goto cleanup;
    }
    mounted_mirror = 1;
  }
  
  umask(S_IWGRP|S_IWOTH);
  retv = fuse_main(args.argc, args.argv, &middfs_oper, NULL);

 cleanup:
  if (mounted_mirror) {
    if (middfs_mirror_unmount(middfs_opts.mirror_dir) < 0) {
      retv = -1;
    }
  }
  fuse_opt_free_args(&args);
  return retv;
}
