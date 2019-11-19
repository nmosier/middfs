/* middfs-conf.c -- configuration file manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <string.h>
#include <search.h>
#include <errno.h>

#include "lib/middfs-conf.h"

#define MIDDFS_ENVVAR_FMT "MIDDFS_%s"
static char *conf_envvar(const char *name) {
   char *envvar = NULL;
   if (asprintf(&envvar, MIDDFS_ENVVAR_FMT, name) < 0) {
      return NULL;      
   }
   return envvar;
}

char *conf_get(const char *name) {
   /* format name of environment variable */
   char *envvar;
   char *val;

   if ((envvar = conf_envvar(name)) == NULL) {
      return NULL;
   }

   fprintf(stderr, "envvar=%s\n", envvar);

   if ((val = getenv(envvar)) == NULL) {
      errno = EINVAL;
      perror("getenv");
   }

   free(envvar);
   
   return val;
}

int conf_set(const char *name, const char *value, int overwrite) {
   /* STUB */
   abort();
}

int conf_put(const char *string) {
   char *envstr;

   
   if ((envstr = conf_envvar(string)) == NULL) {
      return -1;
   }

   fprintf(stderr, "conf_put: ``%s''\n", envstr);
   
   if (putenv(envstr) < 0) {
      free(envstr);
      return -1;
   }

   return 0;
}

int conf_unset(const char *name) {
   /* STUB*/
   abort();
}

#define CONF_LOAD_MAXLINE 1024
int conf_load(const char *path) {
   int retv = 0;
   FILE *fconf;

   /* open configuration file */
   if ((fconf = fopen(path, "r")) == NULL) {
      return -1;
   }

   char line[CONF_LOAD_MAXLINE];
   while (fgets(line, CONF_LOAD_MAXLINE, fconf) != NULL) {
      /* strip comment & newline */
      line[strcspn(line, "#\n")] = '\0';

      /* put conf var */
      if (conf_put(line) < 0) {
         return -1;
      }
   }

   if (ferror(fconf) != 0) {
      retv = -1;
   }

   /* cleanup */
   if (fclose(fconf) < 0) {
      retv = -1;
   }

   return retv;
}
