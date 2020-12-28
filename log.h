#include <stdio.h>
#include <stdlib.h>

#ifndef _LOG_H
#define _LOG_H

enum log_level_id
{
	FATAL = 0,
	ERROR,
	WARN,
	INFO,
	DEBUG
};

#ifdef _DEBUG 
	#define LOG(level, format, args...) \
		do \
		{ \
			if (level < user_option->log_level) { \
				switch (level) \
				{ \
					case DEBUG: \
						printf("[DEBUG] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						fprintf(server_log, "[DEBUG] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						break; \
					case INFO: \
						printf("[INFO] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						fprintf(server_log, "[INFO] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						break; \
					case WARN: \
						printf("[WARN] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						fprintf(server_log, "[WARN] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						break; \
					case ERROR: \
						printf("[ERROR] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						fprintf(server_log, "[ERROR] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						break; \
					default: \
						printf("[FATAL] [%s:%d] " format, __FILE__, __LINE__, ##args); \
						fprintf(server_log, "[FATAL] [%s:%d] " format, __FILE__, __LINE__, ##args); \
				} \
			} \
		} \
		while (0)
#else
	#define LOG(level, format, args...)
#endif

#ifdef _DEBUG
FILE* init_log(const char*);
void close_log(FILE*);
#endif

#endif
