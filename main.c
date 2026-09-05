#define _GNU_SOURCE

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static char *prog_name;

[[noreturn]] static void exit_error(int line, char *msg) {
  fprintf(stderr, "[ERROR %s] line %d: %s\n", prog_name, line, msg);
  _exit(1);
}

[[noreturn]] static void exit_errno(int line) { exit_error(line, strerror(errno)); }

static unsigned int get_wait_second() {
  char *s = getenv("PID1_WAIT_SECOND");
  if (s) {
    int i = atoi(s);
    if (i > 0) return (unsigned int)i;
  }
  return 5;
}

static char *get_wait_path() {
  char *wait_path = getenv("PID1_WAIT_PATH");
  if (!wait_path) wait_path = "/run/pid1.wait";
  return wait_path;
}

[[noreturn]] static void exec_child_or_exit_error(int line, char **argv) {
  execvp(argv[0], argv);
  exit_errno(line);
}

static void set_handler(int sig, void (*handler)(int)) {
  if (sigaction(sig, &(struct sigaction){.sa_handler = handler}, nullptr) == -1) exit_errno(__LINE__);
}

static volatile sig_atomic_t alarm_timeout = false;

static void alarm_handler(int sig) {
  (void)sig;
  alarm_timeout = true;
}

static bool kill_all_and_wait_till_complete() {
  kill(-1, SIGCONT);
  kill(-1, SIGTERM);

  char *wait_path = get_wait_path();

  set_handler(SIGALRM, alarm_handler);

  bool completed = false;
  while (true) {
    int unlink_ret = unlink(wait_path);
    bool unlink_ok = unlink_ret == 0 || (unlink_ret == -1 && errno == ENOENT);
    alarm(get_wait_second());
    alarm_timeout = false;
    while (!alarm_timeout) {
      if (wait(nullptr) == -1 && errno == ECHILD) {
        completed = true;
        break;
      }
    }
    if (!unlink_ok || completed || stat(wait_path, &(struct stat){}) == -1) break;
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
  while (waitpid(-1, nullptr, WNOHANG) > 0);
}

[[noreturn]] static void main_pause() {
  set_handler(SIGINT, main_pause_quit_handler);
  set_handler(SIGTERM, main_pause_quit_handler);
  set_handler(SIGCHLD, main_pause_sigchld_handler);

  while (!quit) pause();

  set_handler(SIGINT, SIG_DFL);
  set_handler(SIGTERM, SIG_DFL);
  set_handler(SIGCHLD, SIG_DFL);

  if (getpid() == 1) {
    _exit(kill_all_and_wait_till_complete() ? EXIT_SUCCESS : EXIT_FAILURE);
  } else {
    _exit(EXIT_SUCCESS);
  }
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

[[noreturn]] static void main_with_child(char **argv) {
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

  set_handler(SIGWINCH, SIG_DFL);
  set_handler(SIGUSR2, SIG_DFL);
  set_handler(SIGUSR1, SIG_DFL);
  set_handler(SIGTERM, SIG_DFL);
  set_handler(SIGQUIT, SIG_DFL);
  set_handler(SIGINT, SIG_DFL);
  set_handler(SIGHUP, SIG_DFL);

  set_handler(SIGCHLD, SIG_DFL);

  if (getpid() == 1) {
    kill_all_and_wait_till_complete();

    if (WIFEXITED(wstatus)) _exit(WEXITSTATUS(wstatus));
    kill(getpid(), WTERMSIG(wstatus));
    _exit(128 + WTERMSIG(wstatus));
  } else {
    _exit(EXIT_SUCCESS);
  }
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
      fprintf(stderr, "[WARNING]: %s will not working unless running as pid 1, will exec directly\n", prog_name);
      exec_child_or_exit_error(__LINE__, argv);
    }
  }

  exit_error(__LINE__, "DEAD CODE !!");
}
