#include "opt.h"

struct user_option *user_option;

struct http_entry server_type[] =
{
	{0, 0},
	{SINGLE_THR_SERVER, "single_thr_server"},
	{MULTI_THR_SERVER, "multi_thr_server"},
	{0, 0}
};

struct user_option* opt_parser(int argc, char** argv)
{
	// init for user_option
	struct user_option* user_opt = malloc(sizeof(struct user_option));
	bzero(user_opt, sizeof(struct user_option));
	// default value setup
	// no_argument
	user_opt->server_type = SINGLE_THR_SERVER;
	// required_argument
	user_opt->conn_num = 1000;
	user_opt->port_num = 8001;
	user_opt->thr_num = 4;
	user_opt->log_level = 1;

	struct option option[] =
	{
		// no_argument
		{"help", no_argument, NULL, 'h'},
		// required_argument
		// only long
		{"server-type", required_argument, NULL, 0},
		// both
		{"conn", required_argument, NULL, 'c'},
		{"port", required_argument, NULL, 'p'},
		{"thread", required_argument, NULL, 't'},
		{"debug", required_argument, NULL, 'd'},
	};
	const char *optstring = "hc:p:t:d:";
	int opt_index = -1;
	
	int c;
	while (1)
	{
		c = getopt_long(argc, argv, optstring, option, &opt_index);
		if (c == -1) {
			break;
		}
		switch(c)
		{
			case 'h':
				help_message();
                break;
			case 'c':
				sscanf(optarg, "%u", &(user_opt->conn_num));
				// conn max
				if (user_opt->conn_num > 100000) {
					user_opt->conn_num = 100000;
				}
				// conn min
				if (user_opt->conn_num < 1) {
					user_opt->conn_num = 1;
				}
				break;
			case 'p':
				sscanf(optarg, "%hu", &(user_opt->port_num));
				break;
			case 't':
				sscanf(optarg, "%hhu", &(user_opt->thr_num));
				// multithread max
				if (user_opt->thr_num > 16) {
					user_opt->thr_num = 16;
				}
				// multithread min
				if (user_opt->thr_num < 1) {
					user_opt->thr_num = 1;
				}
				break;
			case 'd':
				sscanf(optarg, "%hhu", &(user_opt->log_level));
				break;
			case '?':
				help_message();
                break;
			case 0:
				// match server_type
				if (0 == strncmp(option[opt_index].name, "server-type", 11)) {
					sscanf(optarg, "%hhu", &(user_opt->server_type));
					if (user_opt->server_type > MULTI_THR_SERVER) {
						user_opt->server_type = MULTI_THR_SERVER;
					}
					if (user_opt->server_type < SINGLE_THR_SERVER) {
						user_opt->server_type = SINGLE_THR_SERVER;
					}
				} else {
                    help_message();
                }
				break;
		}
        // test opt_index print out
		// printf("opt_index=%d\n", opt_index);
	} // End of while loop

	// print out option settings
	// required_argument
	// only long
	printf("server_type=%s\n", server_type[(int) user_opt->server_type].val);
	// both
	printf("conn_num=%u\n", user_opt->conn_num);
	printf("port_num=%hu\n", user_opt->port_num);
	printf("worker_thread=%hhu\n", user_opt->thr_num);
	printf("log_level=%hhu\n", user_opt->log_level);

	return user_opt;
}

void destroy_user_option(struct user_option* user_opt)
{
	free(user_opt);
}
