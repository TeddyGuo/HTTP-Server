#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "http_client.h"
#include "http_entry.h"
#include "sender.h"
#include "ftab.h"
#include "opt.h"
#include "log.h"

extern FILE* server_log;
extern struct user_option* user_option;
extern struct ftab* ftab;
extern struct http_entry http_method[];
extern struct http_entry http_version[];
extern struct http_entry http_hdr[];
extern struct http_entry http_status_code[];

#ifndef _RECEIVER_H
#define _RECEIVER_H

enum http_parser_state_id
{
	// request line
	REQUEST_LINE_STATE,
	REQ_LINE_METHOD_STATE,
	REQ_LINE_TARGET_STATE,
	REQ_LINE_VERSION_STATE,
	// header field
	HEADER_FIELD_STATE,
	HEADER_FIELD_KEY_STATE,
	HEADER_FIELD_VAL_STATE,
	// message body
	MESSAGE_BODY_STATE
};

static inline int check_http_method_invalid_ch(char ch)
{
	// 0x41 = 'A', 0x5A = 'Z'. 0x61 = 'a'. 0x7A = 'z'
	if (ch < 0x41 ||
		(ch > 0x5A && ch < 0x61) ||
		ch > 0x7A) {
		return -1;
	}
	return 0;
}

static inline int check_http_method(struct http_client* data, int len)
{
	int status;
	switch (data->field[0])
	{
		case 'G':
			// GET method
			if (len != 3) {
				break;
			}
			status = strncmp(data->field, "GET", 3);
			if (status == 0) {
				LOG(DEBUG, "http method: GET\n");
				data->req_line.method = GET;
				return 0;
			}
			break;
		case 'H':
			// HEAD method
			if (len != 4) {
				break;
			}
			status = strncmp(data->field, "HEAD", 4);
			if (status == 0) {
				LOG(DEBUG, "http method: HEAD\n");
				data->req_line.method = HEAD;
				return 0;
			}
			break;
	}
	LOG(DEBUG, "http method not allowed\n");
	data->status_code = METHOD_NOT_ALLOWED_405;
	LOG(DEBUG, "field=%s, len=%d\n", data->field, len);
	return -1;
}

static inline int check_origin_form(struct http_client* data)
{
	int status;
	// the first char of origin-form is '/'
	if (data->field[0] != '/') {
		LOG(INFO, "bad request: origin-form uri lack of first /\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// check '/' only or more
	if (data->field[1] == '\0') {
		snprintf(data->req_line.target, TARGET_SIZE, "%s/%s", DEFAULT_PATH, DEFAULT_TARGET);
	} else {
		snprintf(data->req_line.target, TARGET_SIZE, "%s%s", DEFAULT_PATH, data->field);
	}
	LOG(DEBUG, "origin-form: uri=%s\n", data->req_line.target);
	
	// check in ftab firstly in order not to open file again
	status = ftab_get(ftab, data->req_line.target);
	if (status < 0) {
		int fd = open(data->req_line.target, O_RDONLY);
		if (fd < 0) {
			LOG(INFO, "origin-form: target file not found\n");
			data->status_code = NOT_FOUND_404;
		} else {
			status = ftab_put2(ftab, data->req_line.target, fd);
			if (status < 0) {
				LOG(INFO, "origin-form: target internal server error\n");
				data->status_code = INTERNAL_SERVER_ERROR_500;
			}
		}
	}
	return 0;
}

static inline int check_http_version(struct http_client* data)
{
	LOG(DEBUG, "version=%s\n", data->field);
	// now only support HTTP/DIGIT.DIGIT format
	if ( !(
		data->field[0] == 'H' && data->field[1] == 'T' &&
		data->field[2] == 'T' && data->field[3] == 'P' &&
		data->field[4] == '/' && isdigit(data->field[5]) &&
		data->field[6] == '.' && isdigit(data->field[7])
		) ) {
		LOG(INFO, "bad request: http version protocol syntax error\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	if (data->field[5] != '1') {
		LOG(INFO, "http version: version not supported\n");
		data->status_code = HTTP_VERSION_NOT_SUPPORTED_505;
		return 0;
	}
	switch (data->field[7])
	{
		case '0':
			LOG(DEBUG, "http version: HTTP/1.0\n");
			data->req_line.version = HTTP_VER_1_0;
			break;
		// from 1 - 9 use default HTTP/1.1
		default:
			LOG(DEBUG, "http version: default HTTP/1.1\n");
	}
	return 0;
}

static inline int req_line_parser(struct http_client* data, int iter, int end)
{
	int status, idx;
	char ch;
	
	// init field
	idx = 0;
	bzero(data->field, FIELD_SIZE);
	while (iter < end)
	{
		ch = data->rcv[iter];
		if (ch == ' ') {
			++iter;
			break;
		}
		status = check_http_method_invalid_ch(ch);
		if (status == -1) {
			LOG(WARN, "bad request: method invalid char\n");
			data->status_code = BAD_REQUEST_400;
			return -1;
		}
		data->field[idx] = ch;
		++idx;
		++iter;
	}
	// reach end check
	if (iter == end) {
		LOG(WARN, "bad request: method reach end\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// method field too large
	if (idx > FIELD_SIZE) {
		LOG(WARN, "bad request: method too large\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// parse what kind of method
	status = check_http_method(data, idx);

	// init field
	idx = 0;
	bzero(data->field, FIELD_SIZE);
	while (iter < end)
	{
		ch = data->rcv[iter];
		if (ch == ' ') {
			++iter;
			break;
		}
		data->field[idx] = ch;
		++idx;
		++iter;
	}
	// reach end check
	if (iter == end) {
		LOG(WARN, "bad request: method reach end\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// target field too long
	if (idx > FIELD_SIZE) {
		LOG(WARN, "bad request: uri too long\n");
		data->status_code = URI_TOO_LONG_414;
		return -1;
	}
	// check origin-form
	status = check_origin_form(data);
	if (status == -1) {
		return -1;
	}

	// init field
	idx = 0;
	bzero(data->field, FIELD_SIZE);
	while (iter < end)
	{
		ch = data->rcv[iter];
		data->field[idx] = ch;
		++idx;
		++iter;
	}
	// dont need to check end since it should be end
	// version too large
	if (idx > FIELD_SIZE) {
		LOG(WARN, "bad request: version too large\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// check http version
	status = check_http_version(data);
	if (status == -1) {
		return -1;
	}

	// next state
	data->parser_state = HEADER_FIELD_STATE;

	return 0;
}

static inline int check_hdr_key_invalid_ch(char ch)
{
	switch (ch)
	{
		case '"':
		case '(':
		case ')':
		case ',':
		case '/':
		case ';':
		case '<':
		case '=':
		case '>':
		case '?':
		case '@':
		case '[':
		case '\\':
		case ']':
		case '{':
		case '}':
		case ' ':
		case '\t':
		case '\r':
			return -1;
			break;
		// find out the colon for next state
		case ':':
			return 1;
			break;
	}
	return 0;
}

static inline int check_hdr_key(char* data, int len)
{
	// use lowercase since I translate all char into lowercase already
	// dup key check in hdr val parse
	switch (data[0])
	{
		case 'h':
			if (len == 4 && strncmp(data, "host", len) == 0) {
				LOG(DEBUG, "Host key\n");
				return HOST;
			}
			break;
		case 'c':
			if (len == 10 && strncmp(data, "connection", len) == 0) {
				LOG(DEBUG, "Connnectionn key\n");
				return CONN;
			}
			break;
	}
	return 0;
}

static inline int host_parser(struct http_client* data, int len)
{
	// dup check
	if ( data->hdr.host ) {
		LOG(INFO, "bad request: dup host\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	LOG(DEBUG, "Host parse successfully\n");
	data->hdr.host = 1;
	return 0;
}

static inline int conn_parser(struct http_client* data, int len)
{
	if (len == 5 && strncmp(data->field, "close", 5)) {
		if (data->hdr.close == 1) {
			LOG(INFO, "bad request: dup conn close\n");
			data->status_code = BAD_REQUEST_400;
			return -1;
		}
		LOG(INFO, "Connectionn close val\n");
		data->hdr.close = 1;
	} else if (len == 10 && strncmp(data->field, "keep-alive", 10)) {
		if (data->hdr.keepalive == 1) {
			LOG(INFO, "bad request: dup conn keep-alive\n");
			data->status_code = BAD_REQUEST_400;
			return -1;
		}
		LOG(INFO, "Connection keep-alive val\n");
		data->hdr.keepalive = 1;
	}
	LOG(DEBUG, "Connection parse successfully\n");
	return 0;
}

static inline int hdr_val_parse_by_key(struct http_client* data, int key, int len)
{
	switch (key)
	{
		case HOST:
			return host_parser(data, len);
			break;
		case CONN:
			return conn_parser(data, len);
			break;
	}
	return 0;
}

static inline int hdr_field_parser(struct http_client* data, int iter, int end)
{
	int status;
	char ch;

	// check end of request
	if (data->rcv[iter] == '\r' && iter + 1 == end) {
		return 1;
	}
	LOG(DEBUG, "rcv[iter]=%c, iter=%d, rcv[end]=%c, end=%d\n", data->rcv[iter], iter, data->rcv[end], end);
	if (iter == end) {
		return 1;
	}

	// init field
	int idx = 0;
	bzero(data->field, FIELD_SIZE);
	while (iter < end)
	{
		ch = data->rcv[iter];
		if (ch == ':') {
			++iter;
			break;
		}
		status = check_hdr_key_invalid_ch(ch);
		if (status == -1) {
			LOG(WARN, "bad request: header key invalid char\n");
			data->status_code = BAD_REQUEST_400;
			return -1;
		}
		// turn to lowercase in order to cmp
		data->field[idx] = tolower(ch);
		++idx;
		++iter;
	}
	// hdr key end check
	if (iter == end) {
		LOG(WARN, "bad request: hdr key reach end\n");
		data->status_code = BAD_REQUEST_400;
		return -1;
	}
	// header key too large
	if (idx > FIELD_SIZE) {
		LOG(WARN, "bad request: hdr key field too large\n");
		data->status_code = REQUEST_HEADER_FIELDS_TOO_LARGE_431;
		return -1;
	}
	// hdr key parse
	int key = check_hdr_key(data->field, idx);
	
	// init field
	idx = 0;
	bzero(data->field, FIELD_SIZE);
	while (iter < end)
	{
		ch = data->rcv[iter];
		if (ch == ' ' || ch == '\t' || ch == '\r') {
			// ignore the char
		} else {
			// turn to lowercase in order to cmp
			data->field[idx] = tolower(ch);
			++idx;
		}
		++iter;
	}
	// dont need to check reach end
	// parse hdr val by key
	// plus one due to length usage
	++idx;
	status = hdr_val_parse_by_key(data, key, idx);
	if (status == -1) {
		return -1;
	}

	return 0;
}

int http_parser(struct http_client*);
int recv_process(struct http_client*);

#endif
