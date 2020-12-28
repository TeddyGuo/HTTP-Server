#include "sender.h"

int gen_res(struct http_client* data)
{
	struct res res = {0};
	int status;

	// Date header field
	time_t now = time(NULL);
	struct tm tm = *gmtime(&now);
	strftime(data->field, FIELD_SIZE, "%a, %d %b %Y %H:%M:%S %Z", &tm);
	// check status_code for the request in order to get file
	res.in_fd = get_status_file(data);
	struct stat info;
	status = fstat(res.in_fd, &info);
	if (status < 0) {
		LOG(ERROR, "gen response: fstat error\n");
	}

    // info.st_size = off_t
    // off_t in linux and in MacOS has different raw type
#if defined __linux__
	snprintf(res.hdr, SEND_SIZE, "%s %d %s\r\nDate: %s\r\nServer: Guo/1.0\r\nContent-Length: %ld\r\nContent-Type: text/html\r\n",
				http_version[data->req_line.version].val,
				http_status_code[data->status_code].key,
				http_status_code[data->status_code].val,
				data->field, info.st_size
				);
#elif defined __APPLE__ && __MACH__
	snprintf(res.hdr, SEND_SIZE, "%s %d %s\r\nDate: %s\r\nServer: Guo/1.0\r\nContent-Length: %lld\r\nContent-Type: text/html\r\n",
				http_version[data->req_line.version].val,
				http_status_code[data->status_code].key,
				http_status_code[data->status_code].val,
				data->field, info.st_size
				);
#endif
	// connection check for version
	switch (data->req_line.version)
	{
		// bad request 400 is the only status code that I will need to close the connection now
		case HTTP_VER_1_0:
			if (data->hdr.keepalive == 1 && data->status_code != BAD_REQUEST_400) {
				strcat(res.hdr, "Connection: Keep-Alive\r\n");
			} else {
				data->quit = 1;
			}
			break;
		// 1.1 or over
		default:
			if (data->status_code == BAD_REQUEST_400 || data->hdr.close == 1) {
				strcat(res.hdr, "Connection: Close\r\n");
				data->quit = 1;
			}
	}

    // End of response header CRLF
    strcat(res.hdr, "\r\n");

	// ready to enqueue
	res.size = strlen((res.hdr) );
	
	// check for method like HEAD
	// make it become 0 in order not to send the payload
	if (data->req_line.method == HEAD) {
		res.in_fd = 0;
	}

	// check enqueue
	status = res_enq(&(data->res_q), (void*) &res);
	if (status == -1) {
		// close the recv operation
		data->quit = 1;
	}
	// if quit, we dont need to clear the data for the next request
	if (0 == data->quit) {
		// clear the data for the next request
		clear_http_client(data);
	}
	return send_process(data);
}

int send_process(struct http_client* data)
{
	int status;

	ssize_t wbyte;
	struct res* res;

	res = res_peek(&(data->res_q) );
	// check empty
	if (res == NULL) {
		return 0;
	}

	switch (res->state)
	{
		case RES_HDR_STATE:
		{
			while (res->size > 0)
			{
				wbyte = send(data->fd, res->hdr + res->off, res->size, 0);
				if (wbyte < 0) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						// send next time
						// schedule to send the rest of the file
#ifdef __linux__
						struct epoll_event event;
						event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
						event.data.ptr = (void*) data;
						status = epoll_ctl(data->ctl_fd, EPOLL_CTL_ADD, data->fd, &event);
						if (status == -1) {
							LOG(WARN, "http_sender: send epoll_ctl error=%s", strerror(errno) );
							return -1;
						}
#elif defined __APPLE__ && __MACH__
						struct kevent event;
						EV_SET(&event, data->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, (void*) data);
                        status = kevent(data->ctl_fd, &event, 1, NULL, 0, NULL);
						if (status == -1) {
							LOG(WARN, "http_sender: send kevent error=%s", strerror(errno) );
							return -1;
						}
#endif
						return 0;
					}
					LOG(WARN, "http_sender: send errno=%s\n", strerror(errno) );
					return -1;
				}
				res->size -= wbyte;
				res->off += wbyte;
			}
			// check the method like HEAD in order not to sendfile
			if (0 == res->in_fd) {
				goto send_process_end_stage;
			}
			res->state = 1;
			res->off = 0;
			struct stat info;
			int status = fstat(res->in_fd, &info);
			if (status < 0) {
				LOG(ERROR, "http_sender: fstat error\n");
				return -1;
			}
			res->size = info.st_size;
		}
		case RES_BODY_STATE:
		{
			while (res->size > 0)
			{
#ifdef __linux__
				wbyte = sendfile(data->fd, res->in_fd, &(res->off), res->size);
				LOG(DEBUG, "sendfile wbyte=%ld\n", wbyte);
				if (wbyte == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						// sendfile next time
						// schedule to send the rest of the file
						struct epoll_event event;
						event.events = EPOLLOUT | EPOLLET | EPOLLONESHOT;
						event.data.ptr = (void*) data;
						status = epoll_ctl(data->ctl_fd, EPOLL_CTL_ADD, data->fd, &event);
						if (status == -1) {
							LOG(WARN, "http_sender: sendfile epoll_ctl error=%s", strerror(errno) );
							return -1;
						}
						return 0;
					}
					LOG(WARN, "http_sender: sendfile errno=%s\n", strerror(errno) );
					return -1;
				}
				// res->off will modified by sendfile in linux
				res->size -= wbyte;
                // off_t in linux and in MacOS has different raw type
				LOG(DEBUG, "res->size=%lu, res->off=%ld\n", res->size, res->off);
#elif defined __APPLE__ && __MACH__
				off_t len;
				status = sendfile(res->in_fd, data->fd, res->off, &(len), NULL, 0);
				LOG(DEBUG, "sendfile len=%lld\n", len);
				if (status == -1) {
					if (errno == EAGAIN || errno == EWOULDBLOCK) {
						// sendfile next time
						// schedule to send the rest of the file
						struct kevent event;
						EV_SET(&event, data->fd, EVFILT_WRITE, EV_ADD | EV_ONESHOT, 0, 0, (void*) data);
                        status = kevent(data->ctl_fd, &event, 1, NULL, 0, NULL);
						if (status == -1) {
							LOG(WARN, "http_sender: sendfile kevent error=%s", strerror(errno) );
							return -1;
						}
						return 0;
					}
					LOG(WARN, "http_sender: sendfile errno=%s\n", strerror(errno) );
					return -1;
				}
				res->off += len;
				res->size -= len;
				LOG(DEBUG, "res->size=%lu, res->off=%lld\n", res->size, res->off);
#endif
			}
			break;
		}
	}

send_process_end_stage:
	LOG(DEBUG, "response header\n%s\n", res->hdr);
	// dequeue here
	res = res_deq(&(data->res_q) );
	return data->quit ? -1 : 0 ;
}
