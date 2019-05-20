#define _GNU_SOURCE

#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

static char *prog_name;

static volatile sigset_t set_block;
static volatile sigset_t set_normal;
static volatile int last_sig = 0;

// forwarded signal
static const int fwd_list[] = {
	SIGHUP,
	SIGINT,
	SIGQUIT,
	SIGTERM,
	SIGUSR1,
	SIGUSR2,
	SIGWINCH,
};

// unpause signal
static const int unpause_list[] = {
	SIGINT,
	SIGTERM,
	SIGCHLD,
};

static const int sig_alrm[] = {
	SIGALRM,
};

// helper function
static void exit_error(int line, char *msg) { fprintf(stderr, "[ERROR %s] line %d: %s\n", prog_name, line, msg); _exit(1); }
static void exit_errno(int line) { exit_error(line, strerror(errno)); }
static void block_signal() { sigprocmask(SIG_SETMASK, (sigset_t*)(&set_block), NULL); }
static void unblock_signals() { sigprocmask(SIG_SETMASK, (sigset_t*)(&set_normal), NULL); }

static void handle_signal(int signo) {
	last_sig = signo;
	block_signal();
}

static void register_signal_handler(const int list[], const int list_len) {
	sigemptyset((sigset_t*)(&set_block));
	for (int i = 0; i < list_len; ++i) sigaddset((sigset_t*)(&set_block), list[i]);
	sigprocmask(SIG_SETMASK, NULL, (sigset_t*)(&set_normal));

	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = handle_signal;
	act.sa_mask = set_block;

	for (int i = 0; i < list_len; ++i) {
		if (sigaction(list[i], &act, NULL) == -1) exit_errno(__LINE__);
	}
}

static void reset_signal_handler(const int list[], const int list_len) {
	struct sigaction act;
	memset(&act, 0, sizeof(struct sigaction));
	act.sa_handler = SIG_DFL;
	for (int i = 0; i < list_len; ++i) {
		if (sigaction(list[i], &act, NULL) == -1) exit_errno(__LINE__);
	}
	unblock_signals();
}

static void kill_all_and_exit(int basic_info_mode, int wstatus) {
	kill(-1, SIGCONT);
	kill(-1, SIGTERM);

	register_signal_handler(sig_alrm, sizeof(sig_alrm) / sizeof(sig_alrm[0]));

	alarm(5);
	while (wait(NULL) != -1);
	int complete = errno == ECHILD;
	alarm(0);

	reset_signal_handler(sig_alrm, sizeof(sig_alrm) / sizeof(sig_alrm[0]));

	if (basic_info_mode) {
		if (!complete) _exit(1);
		_exit(0);
	}

	if (WIFEXITED(wstatus)) _exit(WEXITSTATUS(wstatus));
	char *sig = strsignal(WTERMSIG(wstatus));
	if (sig == NULL) sig = "NULL";
	char buf[512];
	snprintf(buf, sizeof(buf) - 1, "Terminated by signal %s", sig);
	exit_error(__LINE__, buf);
}

static void main_sleep() {
	register_signal_handler(unpause_list, sizeof(unpause_list) / sizeof(unpause_list[0]));

	for (;;) {
		unblock_signals();
		pause();
		if (last_sig != SIGCHLD) {
			reset_signal_handler(unpause_list, sizeof(unpause_list) / sizeof(unpause_list[0]));
			kill_all_and_exit(1, 0);
		}
		while (waitpid(-1, 0, WNOHANG) > 0);
	}
}

static void main_with_child(int argc, char *argv[]) {
	int cpid = fork();
	if (cpid == -1) exit_errno(__LINE__);
	else if (cpid) {
		// parent process

		register_signal_handler(fwd_list, sizeof(fwd_list) / sizeof(fwd_list[0]));

		for (;;) {
			unblock_signals();
			int status;
			int pid = wait(&status);
			if (pid == -1) {
				// forward signal
				if (errno == EINTR) kill(cpid, last_sig);
				else exit_errno(__LINE__);
			} else if (pid == cpid && (WIFEXITED(status) || WIFSIGNALED(status))) {
				reset_signal_handler(fwd_list, sizeof(fwd_list) / sizeof(fwd_list[0]));
				kill_all_and_exit(0, status);
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

	exit_error(__LINE__, "DEAD CODE !!");
}

int main(int argc, char *argv[]) {
	prog_name = argv[0];

	if (getpid() != 1) exit_error(__LINE__, "PID is not 1");

	if (argc == 1) main_sleep();

	main_with_child(argc, argv);
}
