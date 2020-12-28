#include "worker.h"

int fd_queue_init(struct fd_queue* q, int size)
{
	q->size = size;
	q->front = -1;
	q->rear = -1;
	q->arr = (int*) malloc(size * sizeof(int) );
	if (0 == q->arr) {
		return -1;
	}
	bzero(q->arr, size * sizeof(int) );
	return 0;
}

void destroy_fd_queue(struct fd_queue* q)
{
	if (q->arr) {
		free(q->arr);
	}
}

int fd_enq(struct fd_queue* q, int fd)
{
	if (!fd_queue_full(q) ) {
		if (q->front == -1) {
			q->front = 0;
			q->rear = 0;
		} else if (q->rear == (q->size - 1) && q->front != 0) {
			q->rear = 0;
		} else {
			++(q->rear);
		}
		q->arr[q->rear] = fd;
		return 0;
	}
	LOG(DEBUG, "fd_enq overflow\n");
	return -1;
}

int fd_peek(struct fd_queue* q)
{
	if (!(fd_queue_empty(q)) ) {
		return q->arr[q->front];
	}
	LOG(DEBUG, "fd_peek underflow\n");
	return -1;
}

int fd_deq(struct fd_queue* q)
{
	if (!(fd_queue_empty(q))) {
		int fd = q->arr[q->front];
		if (q->front == q->rear) {
			q->front = -1;
			q->rear = -1;
		} else if (q->front == (q->size - 1) ) {
			q->front = 0;
		} else {
			++(q->front);
		}
		return fd;
	}
	LOG(DEBUG, "fd_deq underflow\n");
	return -1;
}

void* worker_thr_func(void* item)
{
	struct fd_queue* fd_queue = (struct fd_queue*) item;
	pthread_t tid = pthread_self();
	int status;
    
    int ctl_fd;
#if defined __linux__
	// epoll file descriptor create
    ctl_fd = epoll_create1(0);
#elif defined __APPLE__ && __MACH__
	// kqueue file descriptor create
    ctl_fd = kqueue();
#endif
    assert(-1 != ctl_fd);
	/* 
		read and write have independent event
		That is the reason why I make event list times 2 to be larger
		minus one since the dirty element will never has write event
	*/
	int max_event = (fd_queue->size * 2) - 1;
#if defined __linux__
    struct epoll_event *event_list = (struct epoll_event*) malloc(max_event * sizeof(struct epoll_event) );
	assert(event_list > 0);
    bzero(event_list, max_event * sizeof(struct epoll_event) );
#elif defined __APPLE__ && __MACH__
    struct kevent *event_list = (struct kevent*) malloc(max_event * sizeof(struct kevent) );
	assert(event_list > 0);
    bzero(event_list, max_event * sizeof(struct kevent) );
#endif

	int nevent, iter;
#if defined __linux__
    // The epoll timeout argument specifies the number of milliseconds
    int epoll_timeout = 1e3; // 1sec
	struct epoll_event event;
#elif defined __APPLE__ && __MACH__
	// kevent timespec just need to be set up once since it won't decay like select
	// For compatible issue, epoll_wait need to be tested out to check the reaction
	struct timespec kevent_ts;
	kevent_ts.tv_sec = 1;
	kevent_ts.tv_nsec = 0;
	struct kevent event;
#endif
	int fd;
	struct http_client* data;
	while (1)
	{
        // worker dequeue in order to register the fd
		worker_deq(ctl_fd, fd_queue);
        // returns number of events
#if defined __linux__
        nevent = epoll_wait(ctl_fd, event_list, max_event, epoll_timeout);
		LOG(INFO, "In tid=%ld, epoll got %d events\n", tid, nevent);
        
        for (iter = 0; iter < nevent; ++iter)
        {
            data = (struct http_client*) event_list[iter].data.ptr;
            LOG(INFO, "In tid=%ld, epoll get event=%d\n", tid, nevent);
            if ((event_list[iter].events & EPOLLERR) ||
                (event_list[iter].events & EPOLLHUP) ||
                !(event_list[iter].events & EPOLLIN) ) {
                /* An error has occured on this fd, or the socket is not
                    ready for reading (why were we notified then?) */
                LOG(WARN, "In tid=%ld, conn_sock epoll error\n", tid);
                sock_close(data);
            } else {
                // read event
                if (event_list[iter].events & EPOLLIN) {
                    // Read to connection socket
                    status = recv_process(data);
                }
                // write event
                if (event_list[iter].events & EPOLLOUT) {
                    // Write to connection socket
                    status = send_process(data);
                    // if error occurs
                    if (status == -1) {
                        sock_close(data);
                    }
                }
            }
        }
#elif defined __APPLE__ && __MACH__
		nevent = kevent(ctl_fd, NULL, 0, event_list, max_event, &kevent_ts);
		LOG(INFO, "In tid=%p, kqueue got %d events\n", &tid->__opaque, nevent);
		
		for (iter = 0; iter < nevent; ++iter)
		{
			fd = (int) event_list[iter].ident;
			data = (struct http_client*) event_list[iter].udata;
			if (event_list[iter].flags & EV_EOF) {
				LOG(INFO, "In tid=%p, disconnect fd=%d\n", &tid->__opaque, fd);
				sock_close(data);
				// Socket is automatically removed from the kq by the kernel.
			} else if (event_list[iter].filter == EVFILT_READ) {
				// Read from socket.
				status = recv_process(data);
			} else if (event_list[iter].filter == EVFILT_WRITE) {
				// Write to connection socket
				status = send_process(data);
				// if error occurs
				if (status == -1) {
					sock_close(data);
				}
			}
		} // End of for loop
#endif
	} // End of main while loop in the current thread

	// free the event_list
	free(event_list);
	// close ctl_fd
	close(ctl_fd);

	pthread_exit(0);
}
