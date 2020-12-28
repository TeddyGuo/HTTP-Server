#ifndef _HTTP_ENTRY_H
#define _HTTP_ENTRY_H

struct http_entry
{
    int key;
    const char* val;
};

enum http_method_key
{
    GET = 1,
    HEAD,
    PUT,
    POST,
    DELETE,
    OPTION,
};

enum http_version_key
{
	HTTP_VER_1_0 = 1,
	HTTP_VER_1_1,
};

enum http_hdr_key
{
	HOST = 1,
	CONN,
};

enum http_status_code_key
{
	OK_200 = 1,
	BAD_REQUEST_400,
	FORBIDDEN_403,
	NOT_FOUND_404,
	METHOD_NOT_ALLOWED_405,
	PAYLOAD_TOO_LARGE_413,
	URI_TOO_LONG_414,
	REQUEST_HEADER_FIELDS_TOO_LARGE_431,
	INTERNAL_SERVER_ERROR_500,
	NOT_IMPLEMENTED_501,
	HTTP_VERSION_NOT_SUPPORTED_505,
};

#endif
