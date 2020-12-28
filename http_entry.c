#include "http_entry.h"

struct http_entry http_method[] =
{
    {0, 0},
    {GET, "GET"},
    {HEAD, "HEAD"},
    {PUT, "PUT"},
    {POST, "POST"},
    {OPTION, "OPTION"},
	{0, 0}
};

struct http_entry http_version[] =
{
	{0, 0},
	{HTTP_VER_1_0, "HTTP/1.0"},
	{HTTP_VER_1_1, "HTTP/1.1"},
	{0, 0}
};

struct http_entry http_hdr[] =
{
	{0, 0},
	{HOST, "Host"},
	{CONN, "Connection"},
	{0, 0}
};

struct http_entry http_status_code[] =
{
	{0, 0},
	{200, "OK"},
	{400, "Bad Request"},
	{403, "Forbidden"},
	{404, "Not Found"},
	{405, "Method Not Allowed"},
	{413, "Payload Too Large"},
	{414, "URI Too Long"},
	{431, "Request Header Fields Too Large"},
	{500, "Internal Server Error"},
	{501, "Not Implemented"},
	{505, "HTTP Version Not Supported"},
	{0, 0}
};
