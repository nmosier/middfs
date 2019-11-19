/* middfs-conf.c -- configuration file manager
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <string.h>
#include <search.h>

#include "lib/middfs-conf.h"

static int conf_cmp(const struct middfs_conf_kv *lhs, const struct middfs_conf_kv *rhs);
static int conf_parse_line(char *line, struct middfs_conf_kv kvs[], size_t kvcount);

/* conf_create() -- parse configuraiton info given keys.
 * NOTE: values in kv list should be initialized (NULL if they don't have predefined values).
 */
#define CONF_CREATE_BUFLEN 1024
int conf_create(const char *confpath, struct middfs_conf_kv kvs[], size_t kvcount) {
  int retv = -1;
  
  /* open file */
  FILE *fconf;
  if ((fconf = fopen(confpath, "r")) == NULL) {
    return -1;
  }

  /* for each line, update correspondng entry in key-value pair list */
  char buf[CONF_CREATE_BUFLEN];
  while (fgets(buf, CONF_CREATE_BUFLEN, fconf) != NULL) {
    if (conf_parse_line(buf, kvs, kvcount) < 0) {
      fprintf(stderr, "warning: invalid line in configuration file:\n> ``%s''\n", buf);
    }
  }

  if (ferror(fconf) != 0) {
    perror("conf_create: fgets");
    retv = -1;
  }

  /* cleanup */
  if (fclose(fconf) < 0) {
    perror("conf_create: fclose");
    retv = -1;
  }
  
  return retv;
}

void conf_delete(struct middfs_conf_kv kvs[], size_t kvcount) {
  for (size_t i = 0; i < kvcount; ++i) {
    free(kvs[i].val);
  }
}

/* conf_parse_line() -- parse single configuration line 
 * RETV: -1 if invalid; 0 if line empty; 1 if found kv pair.
 */
static int conf_parse_line(char *line, struct middfs_conf_kv kvs[], size_t kvcount) {
  char *mid;

  /* strip comment and newline */
  line[strspn(line, "#\n")] = '\0';

  /* strip leading whitespace */
  line = line + strspn(line, " \t");

  if (*line == '\0') {
    return 0; /* empty line */
  }

  /* separate line around '=' sign */
  if ((mid = strchr(line, '=')) == NULL) {
    return -1; /* invalid -- expect kv pair */
  }

  /* assign key-value parameters */
  char *key, *val;
  key = line;
  val = mid + 1;

  /* find key in kvs list */
  struct middfs_conf_kv kv = {.key = key, .val = NULL};
  struct middfs_conf_kv *found;
  if ((found = lfind(&kv, kvs, &kvcount, sizeof(kv),
		     (int (*)(const void *, const void *)) conf_cmp)) == NULL) {
    return 0;
  }

  /* update kv pair */
  if (found->val != NULL) {
    free(found->val);
  }
  found->val = strdup(val);

  return 1;
}

static int conf_cmp(const struct middfs_conf_kv *lhs, const struct middfs_conf_kv *rhs) {
  return strcmp(lhs->key, rhs->key);
}
