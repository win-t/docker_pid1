#define _GNU_SOURCE

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

static char *prog_name;

static sigset_t set_all;
static sigset_t set_normal;

static volatile int last_sig = 0;

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
	SIGCHLD,
};

static void handle_signal(int signo) {
	last_sig = signo;
	sigprocmask(SIG_SETMASK, &set_all, NULL);
}

static void register_signal_handler(int list[], int list_len) {
	sigemptyset(&set_all);
	for (int i = 0; i < list_len; ++i) {
		sigaddset(&set_all, list[i]);
	}
	sigprocmask(SIG_SETMASK, NULL, &set_normal);

	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = handle_signal;
	act.sa_mask = set_all;

	for (int i = 0; i < list_len; ++i) {
		if (sigaction(list[i], &act, NULL) == -1) exit_errno(__LINE__);
	}
}

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	if (getpid() != 1) exit_error(__LINE__, "PID is not 1");

	// sleep mode
	if (argc == 1) {

		register_signal_handler(unpause_list, sizeof(unpause_list) / sizeof(unpause_list[0]));

		for (;;) {
			pause();
			if (last_sig != SIGCHLD) _exit(0);
			waitpid(-1, 0, WNOHANG);
			sigprocmask(SIG_SETMASK, &set_normal, NULL);
		}

		exit_error(__LINE__, "DEAD CODE !!");
	}

	int cpid = fork();
	if (cpid == -1) exit_errno(__LINE__);
	else if (cpid) {
		// parent process

		register_signal_handler(fwd_list, sizeof(fwd_list) / sizeof(fwd_list[0]));

		for (;;) {
			int status;
			int pid = wait(&status);
			if (pid == -1) {
				// forward signal
				if (errno == EINTR) kill(cpid, last_sig);
				else exit_errno(__LINE__);
			} else if (pid == cpid) {
				if (WIFEXITED(status)) _exit(WEXITSTATUS(status));
				exit_error(__LINE__, "Main process is not terminated normally");
			}
			sigprocmask(SIG_SETMASK, &set_normal, NULL);
		}

	} else {
		// child process

		char *new_arg[argc];
		for(int i = 1; i < argc; ++i) new_arg[i - 1] = argv[i];
		new_arg[argc - 1] = 0;

		execvp(new_arg[0], new_arg);
		exit_errno(__LINE__);

	}

	exit_error(__LINE__, "DEAD CODE !!");
}
