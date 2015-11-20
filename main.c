#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <time.h>

static char *prog_name;

static int cpid = 0;

static void sig_forwarder(int signo) {
	kill(cpid, signo);
}

static void exit_error(char *msg) {
	fprintf(stderr, "[ERROR %s] %s\n", prog_name, msg);
	exit(1);
}

static void exit_errno() {
	exit_error(strerror(errno));
}

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	if (getpid() != 1) exit_error("PID is not 1");

	if (argc == 1) exit_error("require at least 1 argument");

	cpid = fork();
	if (cpid == -1) exit_errno();

	if (cpid) {

		if (signal(SIGINT, sig_forwarder) == SIG_ERR) exit_errno();
		if (signal(SIGTERM, sig_forwarder) == SIG_ERR) exit_errno();

		int cstatus = 0;
		int count = 5;
		const struct timespec tenmili = { .tv_sec = 0, .tv_nsec = 10000000 };

		for (;;) {
			int status;
			int pid = wait(&status);

			if (pid == -1) {
				if (errno == ECHILD) {
					nanosleep(&tenmili, NULL);
					count--;
					if (count <= 0) return WEXITSTATUS(cstatus);
				} else {
					exit_errno();
				}
			} else if (pid == cpid) {
				cstatus = status;
			}
		}

	} else {

		execvp(argv[1], &argv[1]);
		exit_errno();

	}

	return 0;
}

