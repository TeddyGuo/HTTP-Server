# :set noexpandtab

all: main.c server.c log.c opt.c ftab.c http_client.c http_entry.c receiver.c sender.c worker.c
	gcc -O3 -pthread -g -D_DEBUG -o a.out main.c server.c log.c opt.c ftab.c http_client.c http_entry.c receiver.c sender.c worker.c
clean:
	rm -rvf *.o *.log
