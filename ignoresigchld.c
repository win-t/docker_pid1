#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

static char *prog_name;
static void exit_error(int line, char *msg) { fprintf(stderr, "[ERROR %s] line %d: %s\n", prog_name, line, msg); _exit(1); }
static void exit_errno(int line) { exit_error(line, strerror(errno)); }

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	signal(SIGCHLD, SIG_IGN);

	if(argc <= 1) return 0;

	execvp(argv[1], argv+1);
	exit_errno(__LINE__);

	return 1;
}
