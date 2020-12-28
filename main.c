#include <stdio.h>
#include <signal.h>

#include "server.h"
#include "log.h"
#include "opt.h"
#include "ftab.h"

extern FILE* server_log;
extern struct user_option* user_option;
extern struct ftab* ftab;

int main(int argc, char** argv)
{
	// due to sigpipe error
	signal(SIGPIPE, SIG_IGN);
	// init log
#ifdef _DEBUG
	server_log = init_log("server.log");
#endif
	// init option
	user_option = opt_parser(argc, argv);
	// init ftab
	ftab = new_ftab();
	// server
	server_selection(user_option->server_type);

	// close log
#ifdef _DEBUG
	close_log(server_log);
#endif
	// del user_option
	destroy_user_option(user_option);
	// del ftab
	del_ftab(ftab);
	return 0;
}
