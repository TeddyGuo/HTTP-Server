#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "http_client.h"
#include "opt.h"
#include "log.h"

extern struct user_option* user_option;
extern FILE* server_log;

#ifndef _FTAB_H
#define _FTAB_H

#define FTAB_SIZE (1024 * 1024)

// element of the hash table's chain list
struct ftab_node
{
	char* key;
	int fd;
	struct ftab_node* next;
};

// hash_table
struct ftab
{
	struct ftab_node** tab;
};

static inline void init_ftab_node(struct ftab_node* node)
{
	node->key = NULL;
	node->fd = 0;
	node->next = NULL;
}

static inline void free_ftab_node(struct ftab_node* node)
{
	if (node) {
		int status = close(node->fd);
		if (status == -1) {
			LOG(WARN, "ftab_node: close fd fail\n");
			// error handle here
		}
		if (node->key) {
			free(node->key);
			node->key = NULL;
		}
		if (node) {
			free(node);
		}
	}
}

// The classic Times33 hash function
static inline unsigned int times_33(char* key)
{
	unsigned int hash = 0;
	while (*key)
	{
		hash = (hash << 5) + hash + *key;
		++key;
	}
	return hash;
}

// init file descriptor like 404 or 405, etc
void init_status_file(struct ftab*, char*);

struct ftab* new_ftab(void);
void del_ftab(struct ftab*);
// insert or update a fd indexed by key
int ftab_put2(struct ftab*, char*, int);
// return fd if found else -1
int ftab_get(struct ftab*, char*);
void ftab_rm(struct ftab*, char*);

#endif
