#ifdef __linux__
	#include <sys/epoll.h>
#elif defined __APPLE__ && __MACH__
	#include <sys/event.h>
#endif

#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>

#include "log.h"
#include "opt.h"
#include "http_client.h"
#include "receiver.h"
#include "sender.h"

extern FILE* server_log;
extern struct user_option* user_option;

#ifndef _WORKER_H
#define _WORKER_H

struct fd_queue
{
	int size;
	int front;
	int rear;
	int* arr;
};

static inline int fd_queue_full(struct fd_queue* q)
{
	if ( (q->rear == (q->size - 1) && q->front == 0) || (q->rear == q->front - 1) ) {
		return 1;
	}
	return 0;
}
static inline int fd_queue_empty(struct fd_queue* q)
{
	if (q->front == -1) {
		return 1;
	}
	return 0;
}

int fd_queue_init(struct fd_queue*, int);
void destroy_fd_queue(struct fd_queue*);
int fd_enq(struct fd_queue*, int);
int fd_peek(struct fd_queue*);
int fd_deq(struct fd_queue*);

// the fd_deq logic in worker thread
static inline int worker_deq(int ctl_fd, struct fd_queue* fd_queue)
{
    int status;
    pthread_t tid;

    int fd;
    struct http_client* data;
    while (1)
    {
        fd = fd_deq(fd_queue);
        if (fd == -1) {
            return 0;
        }
#if defined __linux__
        LOG(INFO, "file desciptor %d in tid=%ld\n", fd, tid);
#elif defined __APPLE__ && __MACH__
        LOG(INFO, "file desciptor %d in tid=%p\n", fd, &tid->__opaque);
#endif
        // new a http_client for the fd
        data = new_http_client(ctl_fd, fd);
        if (data == NULL) {
            LOG(WARN, "new_http_client error\n");
            close(fd);
        } else {
            // Listen on the new socket
#if defined __linux__
            struct epoll_event event;
            event.events = EPOLLIN | EPOLLET;
            event.data.ptr = (void*) data;
            status = epoll_ctl(ctl_fd, EPOLL_CTL_ADD, fd, &event);
            if (status == -1) {
                LOG(WARN, "epoll: conn_sock error\n");
                sock_close(data);
            }
#elif defined __APPLE__ && __MACH__
            struct kevent event;
            EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, (void*) data);
            status = kevent(ctl_fd, &event, 1, NULL, 0, NULL);
            if (status == -1) {
                LOG(WARN, "kevent: conn_sock error\n");
                sock_close(data);
            }
#endif
        }
    } // End of fd_queue dequeue
    // strange state
    return -1; 
}

// thread function
void* worker_thr_func(void*);

#endif
