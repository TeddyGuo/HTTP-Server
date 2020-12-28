#include "ftab.h"

// used for storing the fd of default path
struct ftab* ftab;

void init_status_file(struct ftab* ft, char* code)
{
	int status, fd;
	char path[TARGET_SIZE + 1] = {0};

	snprintf(path, TARGET_SIZE, "%s/%s", DEFAULT_PATH, code);
	
	// check file existence
	status = access(path, F_OK);
	if (status < 0) {
		fd = open(path, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
		int len = strlen(code);
		status = write(fd, code, len);
		if (status < 0) {
			LOG(WARN, "status file %s write error\n", code);
		}
		close(fd);
	}
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		LOG(WARN, "status file %s open error\n", code);
	}

	status = ftab_put2(ft, path, fd);
	if (status < 0) {
		LOG(WARN, "put ftab %s file fail\n", code);
	}
}



struct ftab* new_ftab(void)
{
	struct ftab* ft = malloc(sizeof(struct ftab));
	if (ft == NULL) {
		del_ftab(ft);
		return NULL;
	}
	ft->tab = malloc(sizeof(struct ftab_node*) * FTAB_SIZE);
	if (ft->tab == NULL) {
		del_ftab(ft);
		return NULL;
	}
	bzero(ft->tab, sizeof(struct ftab_node*) * FTAB_SIZE);

	init_status_file(ft, "404");
	init_status_file(ft, "WTF");
	
	return ft;
}

void del_ftab(struct ftab* ft)
{
	if (ft) {
		if (ft->tab) {
			int n = 0;
			while (n < FTAB_SIZE)
			{
				struct ftab_node* p = ft->tab[n];
				struct ftab_node* q = NULL;
				while (p)
				{
					q = p->next;
					free_ftab_node(p);
					p = q;
				}
				++n;
			}
			free(ft->tab);
			ft->tab = NULL;
		}
		free(ft);
	}
}

// insert or update a fd indexed by key
int ftab_put2(struct ftab* ft, char* key, int fd)
{
	int idx = times_33(key) % FTAB_SIZE;
	struct ftab_node* p = ft->tab[idx];
	struct ftab_node* pre = p;

	// if key is already stored, update
	while (p)
	{
		if (strcmp(p->key, key) == 0) {
			p->fd = fd;
			break;
		}
		pre = p;
		p = p->next;
	}
	// if key has not been stored, add it
	if (p == NULL) {
		unsigned int klen = strlen(key) + 1;
		char* kstr = malloc(klen);
		bzero(kstr, klen);
		if (kstr == NULL) {
			return -1;
		}
		struct ftab_node* node = malloc(sizeof(struct ftab_node));
		if (node == NULL) {
			free(node);
			kstr = NULL;
			return -1;
		}
		init_ftab_node(node);
		snprintf(kstr, klen, "%s", key);
		node->key = kstr;
		node->fd = fd;

		if (pre == NULL) {
			ft->tab[idx] = node;
		} else {
			pre->next = node;
		}
	}
	return 0;
}

// return fd if found else -1
int ftab_get(struct ftab* ft, char* key)
{
	int idx = times_33(key) % FTAB_SIZE;
	struct ftab_node* p = ft->tab[idx];
	while (p)
	{
		if (strcmp(key, p->key) == 0) {
			return p->fd;
		}
	}
	return -1;
}

void ftab_rm(struct ftab* ft, char* key)
{
	int idx = times_33(key) % FTAB_SIZE;

	struct ftab_node* p = ft->tab[idx];
	struct ftab_node* pre = p;
	while (p)
	{
		if (strcmp(key, p->key) == 0) {
			free_ftab_node(p);
			if (p == pre) {
				ft->tab[idx] = NULL;
			} else {
				pre->next = p->next;
			}
		}
		pre = p;
		p = p->next;
	}
}
