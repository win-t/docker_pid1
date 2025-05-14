#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

static char *prog_name;

static _Noreturn void exit_error(int line, char *msg) {
  fprintf(stderr, "[ERROR %s] line %d: %s\n", prog_name, line, msg);
  _exit(1);
}

static _Noreturn void exit_errno(int line) { exit_error(line, strerror(errno)); }

static int get_wait_second(void) {
  char *s = getenv("PID1_WAIT_SECOND");
  if (s) {
    int i = atoi(s);
    if (i > 0) return i;
  }
  return 5; // default
}

static _Noreturn void exec_child_or_exit_error(int line, char **argv) {
  execvp(argv[0], argv);
  exit_errno(line);
}

static void set_handler(int sig, void (*handler)(int)) {
  sigaction(sig, &(struct sigaction){.sa_handler = handler}, NULL);
}

static volatile sig_atomic_t alarm_timeout = false;

static void alarm_handler(int sig) {
  (void)sig;
  alarm_timeout = true;
}

static int kill_all_and_wait_till_complete(void) {
  kill(-1, SIGCONT);
  kill(-1, SIGTERM);

  set_handler(SIGALRM, alarm_handler);
  alarm(get_wait_second());

  int completed = false;
  while (!alarm_timeout) {
    if (wait(NULL) == -1 && errno == ECHILD) {
      completed = true;
      break;
    }
  }

  alarm(0);
  set_handler(SIGALRM, SIG_DFL);

  return completed;
}

static volatile sig_atomic_t quit = false;

static void main_pause_quit_handler(int sig) {
  (void)sig;
  quit = true;
}

static void main_pause_sigchld_handler(int sig) {
  (void)sig;
  while (waitpid(-1, NULL, WNOHANG) > 0);
}

static _Noreturn void main_pause(void) {
  set_handler(SIGINT, main_pause_quit_handler);
  set_handler(SIGTERM, main_pause_quit_handler);
  set_handler(SIGCHLD, main_pause_sigchld_handler);

  while (!quit) pause();

  set_handler(SIGINT, SIG_DFL);
  set_handler(SIGTERM, SIG_DFL);
  set_handler(SIGCHLD, SIG_DFL);

  _exit(kill_all_and_wait_till_complete() ? 0 : 1);
}

static pid_t cpid;

static void forward_sig_handler(int sig) { kill(cpid, sig); }

static volatile int cpid_status;

static void main_with_child_sigchld_handler(int sig) {
  (void)sig;
  while (true) {
    int status;
    pid_t wpid = waitpid(-1, &status, WNOHANG);
    if (wpid <= 0) break;
    if (wpid == cpid && (WIFEXITED(status) || WIFSIGNALED(status))) {
      cpid_status = status;
      quit = true;
    }
  }
}

static _Noreturn void main_with_child(char **argv) {
  for (int fd = 0; fd < 3; fd++) ioctl(fd, TIOCNOTTY);

  cpid = fork();
  if (cpid == -1) {
    exit_errno(__LINE__);
  } else if (!cpid) { // child process
    setsid();
    for (int fd = 0; fd < 3; fd++) ioctl(fd, TIOCSCTTY, 1);
    exec_child_or_exit_error(__LINE__, argv);
    exit_error(__LINE__, "DEAD CODE !!");
  }

  set_handler(SIGCHLD, main_with_child_sigchld_handler);
  main_with_child_sigchld_handler(SIGCHLD); // to handle the case when child process is already dead

  set_handler(SIGHUP, forward_sig_handler);
  set_handler(SIGINT, forward_sig_handler);
  set_handler(SIGQUIT, forward_sig_handler);
  set_handler(SIGTERM, forward_sig_handler);
  set_handler(SIGUSR1, forward_sig_handler);
  set_handler(SIGUSR2, forward_sig_handler);
  set_handler(SIGWINCH, forward_sig_handler);

  while (!quit) pause();
  int wstatus = cpid_status;

  set_handler(SIGHUP, SIG_DFL);
  set_handler(SIGINT, SIG_DFL);
  set_handler(SIGQUIT, SIG_DFL);
  set_handler(SIGTERM, SIG_DFL);
  set_handler(SIGUSR1, SIG_DFL);
  set_handler(SIGUSR2, SIG_DFL);
  set_handler(SIGWINCH, SIG_DFL);

  set_handler(SIGCHLD, SIG_DFL);

  kill_all_and_wait_till_complete();

  if (WIFEXITED(wstatus)) _exit(WEXITSTATUS(wstatus));
  kill(getpid(), WTERMSIG(wstatus));
  _exit(128 + WTERMSIG(wstatus));
}

int main(int argc, char **argv) {
  prog_name = argv[0];

  for (int i = 1; i < argc; i++) {
    if (strcmp("--", argv[i]) == 0) {
      argc -= i;
      argv += i;
      break;
    }
  }
  argc--;
  argv++;

  if (argc == 0) {
    main_pause();
  } else {
    if (getpid() == 1) {
      main_with_child(argv);
    } else {
      fprintf(stderr, "[WARNING]: %s will not working unless running as pid 1\n", prog_name);
      exec_child_or_exit_error(__LINE__, argv);
    }
  }

  exit_error(__LINE__, "DEAD CODE !!");
}
