#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

static char *prog_name;

static volatile int cpid = 0;

static void exit_error(int line, char *msg) {
	fprintf(stderr, "[ERROR %s] line %d: %s\n", prog_name, line, msg);
	_exit(1);
}

static void exit_errno(int line) {
	exit_error(line, strerror(errno));
}

// forwarded signal
static int fwd_list[] = {
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGWINCH,
};

// unpause signal
static int unpause_list[] = {
	SIGINT,
	SIGTERM,
};

static void handle_signal(int signo) {
	if (cpid != 0) kill(cpid, signo);
}

static void register_signal_handler(int list[], int list_len) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = handle_signal;
	act.sa_flags = SA_RESTART;

	for (int i = 0; i < list_len; ++i) {
		if (sigaction(list[i], &act, NULL) == -1) exit_errno(__LINE__);
	}
}

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	if (getpid() != 1) exit_error(__LINE__, "PID is not 1");

	int sleep_mode = (argc == 1);
	if (!sleep_mode) {
		cpid = fork();
		if (cpid == -1) exit_errno(__LINE__);
	}

	if (sleep_mode) {

		register_signal_handler(unpause_list, sizeof(unpause_list) / sizeof(unpause_list[0]));
		pause();
		_exit(0);

	} else if (cpid) {
		// parent process

		register_signal_handler(fwd_list, sizeof(fwd_list) / sizeof(fwd_list[0]));

		int cstatus = 0;

		for (;;) {
			int status;
			int pid = wait(&status);

			if (pid == -1) {
				// no child available anymore, just exit with saved exit code
				if (errno == ECHILD) _exit(WEXITSTATUS(cstatus));
				else exit_errno(__LINE__);
			} else if (pid == cpid) {
				// direct child, save the exit code
				cpid = 0;
				cstatus = status;
			}
		}

	} else {
		// child process

		char *new_arg[argc];
		for(int i = 1; i < argc; ++i) new_arg[i - 1] = argv[i];
		new_arg[argc - 1] = 0;

		execvp(new_arg[0], new_arg);
		exit_errno(__LINE__);

	}

	// never reach here
	exit_error(__LINE__, "DEAD CODE !!");
}
