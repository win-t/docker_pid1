#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static char *prog_name;

static volatile int cpid = 0;

static void exit_error(char *msg) {
	fprintf(stderr, "[ERROR %s] %s\n", prog_name, msg);
	_exit(1);
}

static void exit_errno() {
	exit_error(strerror(errno));
}

// forwarded signal
static int siglist[] = {
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGWINCH,
};

static void forward_signal(int signo) {
	if (cpid != 0) kill(cpid, signo);
}

static void register_handler() {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = forward_signal;
	act.sa_flags = SA_RESTART;

	for (int i = 0; i < sizeof(siglist) / sizeof(siglist[0]); ++i) {
		if (sigaction(siglist[i], &act, NULL) == -1) exit_errno();
	}
}

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	if (getpid() != 1) exit_error("PID is not 1");

	if (argc == 1) exit_error("No command to be executed, require at least 1 argument");

	cpid = fork();
	if (cpid == -1) exit_errno();

	if (cpid) {
		// parent process

		register_handler();

		int cstatus = 0;

		for (;;) {
			int status;
			int pid = wait(&status);

			if (pid == -1) {
				// no child available anymore, just exit with saved exit code
				if (errno == ECHILD) _exit(WEXITSTATUS(cstatus));
				else exit_errno();
			} else if (pid == cpid) {
				// direct child, save the exit code
				cpid = 0;
				cstatus = status;
			}
		}

	} else {
		// child process

		execvp(argv[1], &argv[1]);
		exit_errno();

	}

	// never reach here
	exit_error("DEAD CODE !!");
}
