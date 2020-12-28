#include "receiver.h"

int http_parser(struct http_client* data)
{
    int status;

    int iter = 0, off, end;
	int rcv_len = strlen(data->rcv);
    char* rcv = data->rcv;
    while (iter < rcv_len)
    {
        off = strcspn(rcv, "\n");
        end = iter + off;
        if (end == rcv_len) {
            break;
        }

        switch (data->parser_state)
        {
            case REQUEST_LINE_STATE:
                // the return val from req_line_parser must be 0 or -1
                status = req_line_parser(data, iter, end);
                if (status == -1) {
                    return gen_res(data);
                }
                break;
            case HEADER_FIELD_STATE:
                // the return val from hdr_field_parser can be 0, -1, or 1
                // 0 stands for success
                // -1 stands for failure
                // 1 stands for one request completion
                status = hdr_field_parser(data, iter, end);
                switch (status)
                {
                    case -1:
                        return gen_res(data);
                        break;
                    case 1:
                        status = gen_res(data);
                        if (status == -1) {
                            return -1;
                        }
                        break;
                }
                break;
        }
        ++off;
        rcv += off;
        iter += off;
    }	
    // move once
    data->rcv_off = strlen(rcv);
    snprintf(data->rcv, data->rcv_off, "%s", rcv);
    bzero(data->rcv + data->rcv_off, RECV_SIZE - data->rcv_off);
    return 0;
}

int recv_process(struct http_client* data)
{
	int status;
	ssize_t rbyte;
    // In edge trigger mode, we need to receive all if we are informed by epoll
#if defined __linux__
    while (1)
    {
#endif
        rbyte = recv(data->fd, (data->rcv + data->rcv_off),
                    (RECV_SIZE - data->rcv_off), 0);
        switch (rbyte)
        {
            case 0:
                /* End of file. The remote has closed the
                    connection. */
                sock_close(data);
                return -1;
                break;
            case -1:
                /* If errno == EAGAIN, that means we have read all
                    data. So go back to the main loop. */
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    LOG(INFO, "recv eagain\n");
                    return 0;
                } else {
                    LOG(WARN, "recv errno=%s\n", strerror(errno) );
                    sock_close(data);
                    return -1;
                }
                break;
            default:
                // http_parser
                status = http_parser(data);
                if (status == -1) {
                    LOG(INFO, "quit state up, close the conn\n");
                    sock_close(data);
                    return -1;
                }
        }
#if defined __linux__
    }
#endif
	return 0;
}
