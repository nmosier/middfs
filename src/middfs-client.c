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

#include "middfs-client.h"
#include "middfs-client-ops.h"
#include "middfs-client-util.h"

#define OPTDEF(t, p) {t, offsetof(struct middfs_opts, p), 1}

/* middfs options 
 * NOTE: Must be global because FUSE API doesn't know about this.
 */

struct middfs_opts {
  int show_help;
  char *root_dir;
  char *mirror_dir;
  char *client_dir;
  char *client_name;
};


/* GLOBALS */

static const struct fuse_opt option_spec[] =
  {/* Required Parameters */
   OPTDEF("--root=%s", root_dir),
   OPTDEF("--mirror=%s", mirror_dir),
   OPTDEF("--dir=%s", client_dir),
   OPTDEF("--name=%s", client_name),
   /* Optional Parameters */
   OPTDEF("--help", show_help),   
   FUSE_OPT_END
  };

static struct middfs_opts middfs_opts;
struct middfs_conf middfs_conf;


void show_help(const char *arg0) {
  fprintf(stderr,
	  "usage: %s [option...] dir\n"					\
	  "Options:\n"							\
	  " --root=<dir>    middfs directory\n"				\
	  " --mirror=<dir>  directory to mount mirror of root dir\n"	\
	  " --dir=<dir>     client directory\n"				\
	  " --help          display this help dialog\n",
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

  if (middfs_opts.client_name == NULL) {
    fprintf(stderr, "%s: required: --name=<path>\n", argv[0]);
    opt_valid = 0;
  } else {
    middfs_conf.client_name = middfs_opts.client_name;
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
