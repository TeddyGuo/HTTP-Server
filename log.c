#include "log.h"

FILE* server_log;

#ifdef _DEBUG

FILE* init_log(const char* path)
{
	FILE* log = fopen(path, "w+");
	if (NULL == log) {
		fprintf(stderr, "log open error\n");
		exit(EXIT_FAILURE);
	}
	return log;
}

void close_log(FILE* log)
{
	fclose(log);
}

#endif
