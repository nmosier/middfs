/* middfs-server-list.h -- list of connected clients.
 * Nicholas Mosier & Tommaso Monaco 2019
 */

#ifndef __MIDDFS_SERVER_LIST_H
#define __MIDDFS_SERVER_LIST_H

/* struct client -- information about a connected client */
struct client {
  char *username;
  char *IP;
};

struct clients {
  struct client *vec;
  size_t len; /* length of calloc(3)ed vector */
  size_t cnt; /* number of used elements in vector */
};

int client_create(const char *username, const char *IP, struct client *client);
void client_delete(struct client *client);
int client_cmp(const struct client *c1, const struct client *c2);


void clients_init(struct clients *clients);
void clients_delete(struct clients *clients);
void clients_remove(size_t index, struct clients *clients);
int clients_add(struct client *client, struct clients *clients);
struct client *client_find(const char *username, const struct clients *clients);


#endif
