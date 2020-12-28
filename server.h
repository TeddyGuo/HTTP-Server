#ifdef __linux__
	#include <sys/epoll.h> // epoll
#elif defined __APPLE__ && __MACH__
	#include <sys/event.h> // kqueue
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <pthread.h>

#include "log.h"
#include "opt.h"
#include "http_entry.h"
#include "http_client.h"
#include "receiver.h"
#include "sender.h"
#include "worker.h"

extern FILE* server_log;
extern struct user_option* user_option;

#ifndef _SERVER_H
#define _SERVER_H

static inline void* get_in_addr(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
static inline unsigned short get_port(struct sockaddr* sa)
{
    if (sa->sa_family == AF_INET) {
        return ntohs(((struct sockaddr_in*)sa)->sin_port);
    }
    
    return ntohs(((struct sockaddr_in6*)sa)->sin6_port);
}

static inline int setnonblocking(int sock)
{
    int flags, status;

    flags = fcntl(sock, F_GETFL, 0);
    if (flags < 0) {
		LOG(ERROR, "fcntl F_GETFL error\n");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    status  = fcntl (sock, F_SETFL, flags);
    if (status < 0)
    {
		LOG(ERROR, "fcntl F_SETFL error\n");
        return -1;
    }

    return 0;
}

static inline int create_listen_sock(unsigned short port_num)
{
    int status, server_sock;

    struct addrinfo hints;
    struct addrinfo *result, *rp;

    bzero(&hints, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    char port[6] = {0};
    snprintf(port, 6, "%d", port_num);
    status = getaddrinfo (NULL, port, &hints, &result);
    if (status != 0)
    {
        LOG(FATAL, "getaddrinfo=%s\n", gai_strerror(status));
        return -1;
    }
	
    for (rp = result; rp != NULL; rp = rp->ai_next)
    {
        server_sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (server_sock < 0) {
            LOG(ERROR, "socket create error\n");
            continue;
        }
	
		// in order to avoid "address already in use" message
		int optval = 1;
		status = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
		if (status < 0) {
			LOG(ERROR, "setsockopt so_reuseaddr error\n");
		}

        status = bind(server_sock, rp->ai_addr, rp->ai_addrlen);
        if (status < 0)
        {
            LOG(ERROR, "bind error\n");
            close(server_sock);
            continue;
        }
		
		LOG(INFO, "server: port=%hu, sockfd=%d\n", get_port(rp->ai_addr), server_sock);
        break;
    }
    if (rp == NULL)
    {
        LOG(FATAL, "bind failure\n");
        return -1;
    }

    freeaddrinfo(result);

    // epoll need to setup server_sock nonblocking in order not to be stuck at sock_accept function and for best performance in ET mode
#if defined __linux__
	// server socket set up nonblocking
    status = setnonblocking(server_sock);
    if (status < 0) {
		LOG(ERROR, "server socket: setnonblocking error\n");
    }
#endif

    int backlog = SOMAXCONN;
	// In the future, we can support backlog option for user maybe
	/*
	if (backlog > SOMAXCONN || backlog < 0) {
		backlog = SOMAXCONN;
	}
	*/
    status = listen(server_sock, backlog);
    if (status < 0) {
        LOG(FATAL, "listen error\n");
        return -1;
    }

    return server_sock;
}

static inline int sock_accept(int server_sock)
{
	int status;

	struct sockaddr addr;
	socklen_t addrlen = sizeof(struct sockaddr);
	int sock;

	sock = accept(server_sock, &addr, &addrlen);
	if (sock == -1) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			LOG(INFO, "sock_accept: no more connection can accept\n");
			return 0;
		}
		LOG(WARN, "accept errno=%s\n", strerror(errno) );
		return -1;
	}
	
	// show info of new conn firstly
	char hbuf[NI_MAXHOST] = {0}, sbuf[NI_MAXSERV] = {0};
	status = getnameinfo(&addr, addrlen,
						hbuf, sizeof(hbuf),
						sbuf, sizeof(sbuf),
						NI_NUMERICHOST | NI_NUMERICSERV);
	if (status == 0) {
		LOG(INFO, "server: accepted conn on descriptor %d "
					"(host=%s, port=%s)\n", sock, hbuf, sbuf);
	}

	// set nonblocking
	status = setnonblocking(sock);

	// return sockfd for registration
	return sock;
}

int single_thr_server(void);
int multi_thr_server(void);
int server_selection(int);

#endif
