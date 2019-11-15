/* middfs-server-list.c -- list of connected clients.
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#include <stdlib.h>
#include <string.h>


#include "lib/middfs-util.h"
#include "server/middfs-client.h"

static int clients_resize(struct clients *clients, size_t newlen);
static void clients_sort(struct clients *clients);

/* CLIENT functions */

int client_create(const char *username, const char *IP, struct client *client) {
  if ((client->username = strdup(username)) == NULL) {
    return -1;
  }
  if ((client->IP = strdup(username)) == NULL) {
    free(client->username);
    return -1;
  }
  return 0;
}

void client_delete(struct client *client) {
  free(client->username);
  free(client->IP);
}

int client_cmp(const struct client *c1, const struct client *c2) {
  return strcmp(c1->username, c2->username);
}


/* CLIENTS functions */

void clients_init(struct clients *clients) {
  memset(clients, 0, sizeof(*clients));
}

void clients_delete(struct clients *clients) {
  /* delete each client entry */
  for (size_t i = 0; i < clients->cnt; ++i) {
    client_delete(&clients->vec[i]);
  }

  /* free vector */
  free(clients->vec);
}

void clients_remove(size_t index, struct clients *clients) {
  /* delete entry */
  client_delete(&clients->vec[index]);
  --clients->cnt;

  if (index < clients->cnt) {
    /* fill entry with back */
    memcpy(&clients->vec[index], &clients->vec[clients->cnt], sizeof(*clients->vec));
    
    /* sort */
    clients_sort(clients);
  }
}

int clients_add(struct client *client, struct clients *clients) {
  /* resize if necessary */
  if (clients->cnt == clients->len) {
    if (clients_resize(clients, MAX(1, clients->len * 2)) < 0) {
      return -1;
    }
  }

  /* copy client into vector */
  memcpy(&clients->vec[clients->cnt++], client, sizeof(*client));

  /* resort vector */
  clients_sort(clients);

  return 0;
}


/* client_find() -- find client given name in clients vector 
 * ARGS:
 *  - username: search for client with this name
 *  - clients: client vector
 * RETV: pointer to entry if found; NULL otherwise.
 */
struct client *client_find(const char *username, const struct clients *clients) {
  const struct client key = {.username = (char *) username};
  return (struct client *) bsearch(&key, clients->vec, clients->cnt, sizeof(key),
				   (int (*)(const void *, const void *)) client_cmp);
}


/* clients_resize() -- resize underlying clients array. 
 * NOTE: This is only for internal use by clients_* functions.
 * NOTE: Deletes any extra elements if downsized. 
 */
static int clients_resize(struct clients *clients, size_t newlen) {
  /* remove any elements that will be truncated */
  while (clients->cnt > newlen) {
    clients_remove(clients->cnt - 1, clients);
  }
  
  /* resize vector */
  struct client *newvec;
  if ((newvec = realloc(clients->vec, newlen * sizeof(*clients->vec))) == NULL) {
    return -1;
  }

  /* update struct members */
  clients->vec = newvec;
  clients->len = newlen;
  return 0;
}

/* clients_sort() -- sort underlying clients array.
 * NOTE: Only for internal use by client_* functions. 
 */
static void clients_sort(struct clients *clients) {
  qsort(clients->vec, clients->cnt, sizeof(*clients->vec), (int (*)(const void *, const void *))
	client_cmp);
}

