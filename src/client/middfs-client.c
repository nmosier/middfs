/* middfs client file system
 * Tommaso Monaco & Nicholas Mosier
 * Sep 2019
 */

#include "client/middfs-client-fuse.h"

#define _GNU_SOURCE /* TODO: Shouldn't need this. Not portable. */

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
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "lib/middfs-serial.h"
#include "lib/middfs-conn.h"
#include "lib/middfs-pkt.h"
#include "lib/middfs-util.h"
#include "lib/middfs-conf.h"

#include "client/middfs-client.h"
#include "client/middfs-client-ops.h"
#include "client/middfs-client-rsrc.h"
#include "client/middfs-client-responder.h"
#include "client/middfs-client-handler.h"
#include "client/middfs-client-conf.h"

#define OPTDEF(t, p) {t, offsetof(struct middfs_opts, p), 1}

#define CLIENT_BACKLOG_DEFAULT 10

static int start_client_responder(const char *port, int backlog, pthread_t *thread);
static int client_connect(const char *server_IP, int port, char *username);

/* middfs options 
 * NOTE: Must be global because FUSE API doesn't know about this.
 */

struct middfs_opts {
   int show_help;
   char *localport;
   char *remoteport;
   char *username;
   char *confpath;
   char *homepath;
};


/* GLOBALS */

static const struct fuse_opt option_spec[] =
   {/* Required Parameters */
    /* Optional Parameters */
    OPTDEF("--localport=%s", localport),
    OPTDEF("--remoteport=%s", remoteport),
    OPTDEF("--conf=%s", confpath),
    OPTDEF("--name=%s", username),
    OPTDEF("--home=%s", homepath),
    OPTDEF("--help", show_help),   
    FUSE_OPT_END
  };

static struct middfs_opts middfs_opts;
// struct middfs_conf middfs_conf;


void show_help(const char *arg0) {
  fprintf(stderr,
          "usage: %s [option...] dir\n"                  \
          "Options:\n"                                   \
          "TODO\n"                                       \
          " --help          display this help dialog\n",
          arg0);
}


void middfs_defaultopts(struct middfs_opts *opts) {
   memset(opts, 0, sizeof(*opts));
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
  
  /* parse options */
  if (fuse_opt_parse(&args, &middfs_opts,
		     option_spec, NULL) < 0) {
    return 1; /* exit */
  }

  if (middfs_opts.show_help) {
     /* show help and exit */     
    show_help(argv[0]);    
    if (fuse_opt_add_arg(&args, "--help") < 0) {
      goto cleanup;
    }
    fuse_main(args.argc, args.argv, &middfs_oper, NULL);
    return 0;
  }

  /* load configuration file */
  char *confpath;

  /* determine conf path to use */
  if ((confpath = middfs_opts.confpath) == NULL) {
     if (asprintf(&confpath, "%s/" MIDDFS_CONF_FILENAME, getenv("HOME")) < 0) {
        perror("asprintf");
        exit(1);
     }
  }
  
  if (access(confpath, R_OK) == 0) {
     /* read in config file */
     fprintf(stderr, "middfs-client: loading configuration from %s\n", confpath);
     if (conf_load(confpath) < 0) {
        perror("conf_load");
        exit(1);
     }
  } else {
     fprintf(stderr, "middfs-client: no configuration file found\n");
  }

#if 0
  /* validate options */
  /* convert to absolute path */
  if ((errno = -middfs_abspath(&middfs_opts.homepath)) > 0) {
     perror("middfs_abspath");
     goto cleanup;
  }
#endif

  /* connect to server */
  if (client_connect(SERVER_IP, LISTEN_PORT_DEFAULT, conf_get(MIDDFS_CONF_USERNAME)) < 0) {
     perror("client_connect");
     goto cleanup;
  }

  /* start client responder */
  pthread_t client_responder_thread;
  int err = 0;
  uint32_t client_responder_port = get_conf_uint32(MIDDFS_CONF_LOCALPORT, &err);
  if (err) {
     perror("get_conf_uint32");
     goto cleanup;
  }
  
  if (start_client_responder(client_responder_port, CLIENT_BACKLOG_DEFAULT,
                             &client_responder_thread) < 0) {
    perror("start_client_responder");
    goto cleanup;
  }
  
  umask(S_IRGRP|S_IROTH|S_IWGRP|S_IWOTH);

  /* set up client listener */
  retv = fuse_main(args.argc, args.argv, &middfs_oper, NULL);

 cleanup:
  fuse_opt_free_args(&args);
  return retv;
}


static int start_client_responder(const char *port, int backlog, pthread_t *thread) {
  int retv = 0;
  
  struct client_responder_args args_tmp =
    {.port = port,
     .backlog = backlog
    };

  struct client_responder_args *args;

  if ((args = malloc(sizeof(*args))) == NULL) {
    perror("malloc");
    return -1;
  }

  memcpy(args, &args_tmp, sizeof(args_tmp));
  
  if ((errno = pthread_create(thread, NULL, (void *(*)(void *)) client_responder, args)) > 0) {
    perror("pthread_create");
    free(args);
    retv = -1;
  }

  return retv;
}

/* client_connect() -- send MPKT_CONNECT packet to server.
 * RETV: -1 on error; 0 on success.
 */
static int client_connect(const char *server_IP, int port, char *username) {
   int retv = -1;
   
   /* obtain socket for communication with server */
   int sockfd = -1;
   if ((sockfd = inet_connect(server_IP, port)) < 0) {
      perror("inet_connect");
      goto cleanup;
   }

   /* construct connection packet */
   struct middfs_packet conn_pkt =
      {.mpkt_magic = MPKT_MAGIC,
       .mpkt_type = MPKT_CONNECT
      };
   struct middfs_connect *conn = &conn_pkt.mpkt_un.mpkt_connect;
   conn->name = username;

   int err = 0;
   conn->port = conf_get_uint32(MIDDFS_CONF_LOCALPORT, &err);
   if (err) {
      fprintf(stderr, "client_connect: invalid local port value ``%s''\n", MIDDFS_CONF_LOCALPORT);
      goto cleanup;
   }
   
   struct buffer buf_out;
   buffer_init(&buf_out);

   if (buffer_serialize(&conn_pkt, (serialize_f) serialize_pkt, &buf_out) < 0) {
      perror("buffer_serialize");
      goto cleanup;
   }
   
   while (!buffer_isempty(&buf_out) && buffer_write(sockfd, &buf_out) >= 0) {}
  
   /* success */
   retv = 0;

 cleanup:
   if (sockfd >= 0) {
      if (close(sockfd) < 0) {
         perror("close");
      }
   }
   
   return retv;
}
