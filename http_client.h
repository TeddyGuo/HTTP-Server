#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "opt.h"
#include "log.h"

extern struct user_option* user_option;
extern FILE* server_log;

#ifndef _HTTP_CLIENT_H
#define _HTTP_CLIENT_H

#define RECV_SIZE 8191 // 8K
#define FIELD_SIZE 255
#define DEFAULT_PATH "."
#define DEFAULT_TARGET "index.html"
#define TARGET_SIZE (32 + FIELD_SIZE)
#define SEND_SIZE 1023 // 1K
#define RES_Q_SIZE 25

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;

struct req_line
{
	unsigned char method;
	char target[TARGET_SIZE + 1];
	unsigned char version;
};

struct hdr_field
{
	unsigned char host: 1;
	unsigned char conn: 1;
	unsigned char keepalive: 1;
	unsigned char close: 1;
};

struct res
{
	// state
	unsigned char state: 1; // only need one bit for both response header and response payload
	// general varibles
	off_t off;
	size_t size;
	// response header
	char hdr[SEND_SIZE + 1];
	// senfile info
	int in_fd;
};

struct res_queue
{
	int front;
	int rear;
	struct res res[RES_Q_SIZE];
};

struct http_client
{
	int ctl_fd; // store fd like kqueue_fd or epoll_fd
	int fd;
	unsigned char quit: 1;
	// pipeline feature
	int rcv_off;
	// int rcv_len;
	// request
	char rcv[RECV_SIZE + 1];
	unsigned char status_code;
	unsigned char parser_state; // record the state of parser
	/* 
		to get the field based on every parser state
		of course, it can be used to tmp usage if you want
	*/
	char field[FIELD_SIZE + 1];
	// request line status
	struct req_line req_line;
	// header field status
	struct hdr_field hdr;
	// response
	struct res_queue res_q;
};

static inline struct http_client* new_http_client(int ctl_fd, int fd)
{
	// malloc a space for the connection
	struct http_client* data = (struct http_client*) malloc(sizeof(struct http_client));
	if (0 == data) {
		return NULL;
	}
	bzero(data, sizeof(struct http_client));
	// init for http_client
	data->ctl_fd = ctl_fd;
	data->fd = fd;
	data->req_line.version = HTTP_VER_1_1;
	data->status_code = OK_200;
	// res_q init
	data->res_q.front = -1;
	data->res_q.rear = -1;
	return data;
}

static inline void clear_http_client(struct http_client* data)
{
	// data->ctl_fd = 0;
	// data->fd = 0;
	// data->quit = 0;
	// data->rcv_off = 0;
	// data->rcv_len = 0;
	// bzero(data->rcv, RECV_SIZE);
	data->status_code = OK_200;
	data->parser_state = 0;
	// bzero(data->field, FIELD_SIZE);
	// bzero(data->req_line, sizeof(struct req_line));
	data->req_line.method = 0;
	data->req_line.version = HTTP_VER_1_1;
	bzero((void*) &(data->hdr), sizeof(struct hdr_field));
}

static inline void sock_close(struct http_client* data)
{
	if (data) {
		LOG(DEBUG, "Closed conn on descriptor %d\n", data->fd);

		/* Closing the descriptor will make epoll remove it
			from the set of descriptors which are monitored. */
		close(data->fd);
		// free struct http_client
		free(data);
	}
}

static inline int res_q_full(struct res_queue* q)
{
	if ( (q->rear == (RES_Q_SIZE - 1) && q->front == 0) || (q->rear == q->front - 1) ) {
		return 1;
	}
	return 0;
}
static inline int res_q_empty(struct res_queue* q)
{
	if (q->front == -1) {
		return 1;
	}
	return 0;
}

int res_enq(struct res_queue*, void*);
struct res* res_peek(struct res_queue*);
struct res* res_deq(struct res_queue*);
void res_q_display(struct res_queue*);

#endif
