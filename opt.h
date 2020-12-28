#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http_entry.h"
#include "log.h"

extern FILE* server_log;

#ifndef _OPT_H
#define _OPT_H

struct user_option
{
	// required_argument
	// only long
	unsigned char server_type;
	// both
	unsigned short port_num;
	unsigned int conn_num;
	unsigned char thr_num;
	unsigned char log_level;
};

enum server_type_id
{
	SINGLE_THR_SERVER = 1,
	MULTI_THR_SERVER
};

static inline void help_message(void)
{
    printf(" option usage:\n");
    printf("==================================================================================================================================\n");
    printf(" short option:\n");
    printf("----------------------------------------------------------------------------------------------------------------------------------\n");
    printf("%5s : %s\n", "-h", "show help message");
    printf("%5s : %s\n", "-c", "set up the total connections can be accepted by the server. max=100,000, min=1. default=1000");
    printf("%5s : %s\n", "-p", "set up the port number. default=8001");
    printf("%5s : %s\n", "-t", "set up the worker thread number, which does not include main thread. max=16, min=1. default=4");
    printf("%5s : %s\n", "-d", "set up the debug level. default=1");
    printf("==================================================================================================================================\n");
    printf(" long option:\n");
    printf("----------------------------------------------------------------------------------------------------------------------------------\n");
    printf("%10s : %s\n", "--server-type", "select the server version, single or multiple thread. default=1 (single)");
    printf("==================================================================================================================================\n");    
    
    exit(EXIT_SUCCESS); 
}

struct user_option* opt_parser(int, char**);
void destroy_user_option(struct user_option*);

#endif
