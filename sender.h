#ifdef __linux__
	#include <sys/epoll.h>
	#include <sys/sendfile.h>
#elif defined __APPLE__ && __MACH__
	#include <sys/event.h>
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>

#include "http_entry.h"
#include "http_client.h"
#include "log.h"
#include "opt.h"
#include "ftab.h"

extern FILE* server_log;
extern struct user_option* user_option;
extern struct ftab* ftab;
extern struct http_entry http_method[];
extern struct http_entry http_version[];
extern struct http_entry http_hdr[];
extern struct http_entry http_status_code[];

#ifndef _SENDER_H
#define _SENDER_H

enum send_process_state_id
{
	RES_HDR_STATE = 0,
	RES_BODY_STATE
};

static inline int get_status_file(struct http_client* data)
{
	int fd;
	unsigned char flag = 1;
	char path[TARGET_SIZE + 1];
	
	while (flag)
	{
		flag = 0;
		switch (data->status_code)
		{
			case OK_200:
			{
				fd = ftab_get(ftab, data->req_line.target);
				if (fd < 0) {
					LOG(ERROR, "file lost due to unknown reason\n");
					data->status_code = NOT_FOUND_404;
					// do the operation again due to error
					flag = 1;	
				}
				break;
			}
			// remember to init in new ftab firstly for the following status code file
			case NOT_FOUND_404:
			{
				snprintf(path, TARGET_SIZE, "%s/404", DEFAULT_PATH);
				fd = ftab_get(ftab, path);
				if (fd < 0) {
					init_status_file(ftab, "404");
					// do the oper again
					flag = 1;
				}
				break;
			}
			default:
			{
				snprintf(path, TARGET_SIZE, "%s/WTF", DEFAULT_PATH);
				fd = ftab_get(ftab, path);
				if (fd < 0) {
					init_status_file(ftab, "WTF");
					// do the oper again
					flag = 1;
				}
			}
		}
	}
	return fd;
}

int gen_res(struct http_client*);
int send_process(struct http_client*);

#endif
