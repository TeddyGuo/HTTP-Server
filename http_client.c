#include "http_client.h"

int res_enq(struct res_queue* q, void* data)
{
    int status;
	struct res* res = (struct res*) data;
	if (!res_q_full(q) ) {
		if (q->front == -1) {
			q->front = 0;
			q->rear = 0;
		} else if (q->rear == (RES_Q_SIZE - 1) && q->front != 0) {
			q->rear = 0;
		} else {
			++(q->rear);
		}
		// data transfer
		q->res[q->rear].state = 0;
		q->res[q->rear].off = 0;
		q->res[q->rear].size = res->size;
        // check for possible truncate issue though I have taken that issue
		status = snprintf( (q->res[q->rear].hdr), SEND_SIZE, "%s", res->hdr);
		if (status < 0) {
            LOG(WARN, "res_enq snprintf has been truncated\n");
            return -1;
        }
        q->res[q->rear].in_fd = res->in_fd;
		return 0;
	}
	LOG(DEBUG, "res_enq overflow\n");
	return -1;
}

struct res* res_peek(struct res_queue* q)
{
	if (!(res_q_empty(q)) ) {
		return &(q->res[q->front]);
	}
	LOG(DEBUG, "res_peek underflow\n");
	return NULL;
}

struct res* res_deq(struct res_queue* q)
{
	if (!(res_q_empty(q))) {
		struct res* data = &(q->res[q->front]);
		if (q->front == q->rear) {
			q->front = -1;
			q->rear = -1;
		} else if (q->front == (RES_Q_SIZE - 1) ) {
			q->front = 0;
		} else {
			++(q->front);
		}
		return data;
	}
	LOG(DEBUG, "res_deq underflow\n");
	return NULL;
}

void res_q_display(struct res_queue* q)
{
	struct res* res;
	if (!(res_q_empty(q)) ) {
		if (q->rear >= q->front) {
			for (int n = q->front; n <= q->rear; ++n)
			{
				res = &(q->res[n]);
				fprintf(stdout, "----------%03d----------\n", n);
				// off_t in different OS has different raw type
#if defined __linux__
				fprintf(stdout, "state=%hhu, off=%ld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#elif defined __APPLE__ && __MACH__
				fprintf(stdout, "state=%hhu, off=%lld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#endif
				fprintf(stdout, "----------hdr---------\n%s\n", res->hdr);
			}
		} else {
			for (int n = q->front; n < RES_Q_SIZE; ++n)
			{
				res = &(q->res[n]);
				fprintf(stdout, "----------%03d----------\n", n);
#if defined __linux__
				fprintf(stdout, "state=%hhu, off=%ld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#elif defined __APPLE__ && __MACH__
				fprintf(stdout, "state=%hhu, off=%lld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#endif
				fprintf(stdout, "----------hdr---------\n%s\n", res->hdr);
			}
			for (int n = 0; n <= q->rear; ++n)
			{
				res = &(q->res[n]);
				fprintf(stdout, "----------%03d----------\n", n);
#if defined __linux__
				fprintf(stdout, "state=%hhu, off=%ld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#elif defined __APPLE__ && __MACH__
				fprintf(stdout, "state=%hhu, off=%lld. size=%lu, in_fd=%d\n",
						res->state, res->off, res->size, res->in_fd);
#endif
				fprintf(stdout, "----------hdr---------\n%s\n", res->hdr);
			}
		}
	}
	fprintf(stderr, "res_q is empty\n");
}
