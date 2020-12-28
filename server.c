#include "server.h"

int single_thr_server(void)
{
	int status;
    
	// server socket create
	int server_sock = create_listen_sock(user_option->port_num);
    assert(-1 != server_sock);
    
    // create the control fd
    int ctl_fd;
#if defined __linux__
    // epoll file descriptor create
    ctl_fd = epoll_create1(0);
#elif defined __APPLE__ && __MACH__
	// kqueue file descriptor create
    ctl_fd = kqueue();
#endif
    assert(-1 != ctl_fd);

    // register the server_sock into the control fd
#if defined __linux__
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_sock;
    status = epoll_ctl(ctl_fd, EPOLL_CTL_ADD, server_sock, &event);
#elif defined __APPLE__ && __MACH__
	struct kevent event;
    EV_SET(&event, server_sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
	status = kevent(ctl_fd, &event, 1, NULL, 0, NULL);
#endif
    assert(-1 != status);
    
	// read and write have independent event
	// That is the reason why I make conn_num times 2
	int max_event = (int) user_option->conn_num * 2;
    // malloc event_list to monitor
#if defined __linux__
    struct epoll_event *event_list = (struct epoll_event*) malloc(max_event * sizeof(struct epoll_event) );
    assert(event_list != 0);
    bzero(event_list, max_event * sizeof(struct epoll_event) );
#elif defined __APPLE__ && __MACH__
    struct kevent *event_list = (struct kevent*) malloc(max_event * sizeof(struct kevent) );
	assert(event_list > 0);
    bzero(event_list, max_event * sizeof(struct kevent) );
#endif

	int nevent, iter;
	int fd;
	struct http_client* data;
	while (1)
	{
        // the main logic of linux epoll
#if defined __linux__
        nevent = epoll_wait(ctl_fd, event_list, max_event, -1);
        if (nevent == -1) {
            LOG(WARN, "epoll_wait get error=%s\n", strerror(errno) );
        }
        for (iter = 0; iter < nevent; ++iter) 
        {
            data = (struct http_client*) event_list[iter].data.ptr;
            if ((event_list[iter].events & EPOLLERR) ||
                (event_list[iter].events & EPOLLHUP) ||
                !(event_list[iter].events & EPOLLIN) ) {
                /* An error has occured on this fd, or the socket is not
                    ready for reading (why were we notified then?) */
                LOG(WARN, "conn_sock epoll error\n");
                sock_close(data);
            } else if (event_list[iter].data.fd == server_sock) {
                // In edge trigger mode, everything will only be informed once
                while ( (fd = sock_accept(server_sock) ) > 0)
                {
                    // new a http_client for the new conn socket
                    data = new_http_client(ctl_fd, fd);
                    if (data == NULL) {
                        LOG(WARN, "new_http_client error\n");
                        close(fd);
                    } else {
                        // Listen on the new socket
                        event.events = EPOLLIN | EPOLLET;
                        // transfer the data ptr
                        event.data.ptr = (void*) data;
                        status = epoll_ctl(ctl_fd, EPOLL_CTL_ADD, fd, &event);
                        if (status == -1) {
                            LOG(WARN, "epoll_ctl: conn_sock error\n");
                            sock_close(data);
                        }
                    }
                }
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
#endif
        // the main logic of MacOS kqueue
#if defined __APPLE__ && __MACH__
        // returns number of events
        nevent = kevent(ctl_fd, NULL, 0, event_list, max_event, NULL);
		if (nevent == -1) {
            LOG(WARN, "kevent get error=%s\n", strerror(errno) );
        }
        for (iter = 0; iter < nevent; ++iter)
		{
            fd = (int) event_list[iter].ident;
            data = (struct http_client*) event_list[iter].udata;
			if (event_list[iter].flags & EV_EOF) {
                LOG(INFO, "Disconnect fd=%d\n", fd);
                sock_close(data);
                // Socket is automatically removed from the kq by the kernel.
            } else if (fd == server_sock) {
				// accept new conn
				fd = sock_accept(server_sock);
				if (fd > 0) {
					// new a http_client for the new conn socket
					data = new_http_client(ctl_fd, fd);
					if (data == NULL) {
						LOG(WARN, "new_http_client error\n");
						close(fd);
					} else {
						// Listen on the new socket
						EV_SET(&event, fd, EVFILT_READ, EV_ADD, 0, 0, (void*) data);
						status = kevent(ctl_fd, &event, 1, NULL, 0, NULL);
						if (status == -1) {
							LOG(WARN, "kevent: conn_sock error\n");
							sock_close(data);
						}
					}
				}
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
        }
#endif
    }
	// close the file descriptors
	close(server_sock);
	close(ctl_fd);
	// free event_list
	free(event_list);

    return 0;
}

int multi_thr_server(void)
{
	int status;
    
	// server socket create
	int server_sock = create_listen_sock(user_option->port_num);
    assert(-1 != server_sock);
    
    int ctl_fd;
#if defined __linux__
    // epoll file descriptor create
    ctl_fd = epoll_create1(0);
#elif defined __APPLE__ && __MACH__
	// kqueue file descriptor create
    ctl_fd = kqueue();
#endif
    assert(-1 != ctl_fd);

    // register the server_sock into the control fd
#if defined __linux__
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = server_sock;
    status = epoll_ctl(ctl_fd, EPOLL_CTL_ADD, server_sock, &event);
#elif defined __APPLE__ && __MACH__
	struct kevent event;
    EV_SET(&event, server_sock, EVFILT_READ, EV_ADD, 0, 0, NULL);
	status = kevent(ctl_fd, &event, 1, NULL, 0, NULL);
#endif
    assert(-1 != status);
    
	// malloc fd_queue space for the whole worker thread
	struct fd_queue* fd_queues = (struct fd_queue*) malloc(user_option->thr_num * sizeof(struct fd_queue) );
	assert(0 != fd_queues);
	bzero(fd_queues, user_option->thr_num * sizeof(struct fd_queue));
	/*
		decide how many fd are there for every fd_queue
		plus one for dirty element in fd_queue
		plus one for integer division issue
	*/
	int fd_size = (user_option->conn_num / user_option->thr_num) + 1 + 1;
	// worker thread
	int iter = 0;
	pthread_t tid[user_option->thr_num];
	while (iter < user_option->thr_num)
	{
		// init fd_queue for the thread
		status = fd_queue_init(&(fd_queues[iter]), fd_size);
		assert(0 == status);
		// create thread
		status = pthread_create(&tid[iter], NULL, worker_thr_func, (void*) &(fd_queues[iter]) );
		assert(0 == status);
		++iter;
	}
	// main thread task
	int fd;
	while (1)
	{
        // returns number of events
#if defined __linux__
		// only server_sock will toggle the epoll_wait in main thread
        status = epoll_wait(ctl_fd, &event, 1, -1);
        if (status == -1) {
            LOG(ERROR, "epoll_wait error\n");
        }
		// accept new conn
		while ( (fd = sock_accept(server_sock) ) > 0)
        {
            // throw the fd into worker fd queue
            fd_enq(&(fd_queues[fd % user_option->thr_num]), fd);
        }
#endif

#if defined __APPLE__ && __MACH__
		// only server_sock will toggle the kevent in main thread
        status = kevent(ctl_fd, NULL, 0, &event, 1, NULL);
        if (status == -1) {
            LOG(ERROR, "kevent error\n");
        }
		// accept new conn
		fd = sock_accept(server_sock);
		if (fd > 0) {
			// throw the fd into worker fd queue
			fd_enq(&(fd_queues[fd % user_option->thr_num]), fd);
		}
#endif
    }
	// recycle the threads resource
	iter = 0;
	while (iter < user_option->thr_num)
	{
		destroy_fd_queue(&(fd_queues[iter]) );
		// you can return something from worker threads if you need it in the future
		pthread_join(tid[iter], NULL);
		++iter;	
	}
	free(fd_queues);
	// close the file descriptors
	close(server_sock);
	close(ctl_fd);

	return 0;
}

int server_selection(int server_type)
{
	switch (server_type)
	{
		case SINGLE_THR_SERVER:
			return single_thr_server();
			break;
		case MULTI_THR_SERVER:
			return multi_thr_server();
			break;
	}
	return 0;
}
