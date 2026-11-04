#define _POSIX_C_SOURCE 200809L
#include "wait2.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#if defined(__linux__)
#include <pty.h>
#elif defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <util.h>
#else
#error "PTY test fixture needs an openpty() header for this host"
#endif

struct fixture {
    int master, slave, command_r, command_w;
    int raw_enabled;
    struct termios original;
};

#define REQUIRE(x) do { \
    if (!(x)) { \
        fprintf(stderr, "    check failed at line %d: %s (errno=%d)\n", \
                __LINE__, #x, errno); \
        return 1; \
    } \
} while (0)

static void fixture_close(struct fixture *f)
{
    if (f->raw_enabled) {
        if (tcsetattr(f->slave, TCSANOW, &f->original) < 0)
            perror("tcsetattr restore");
        f->raw_enabled = 0;
    }
    if (f->master >= 0) close(f->master);
    if (f->slave >= 0) close(f->slave);
    if (f->command_r >= 0) close(f->command_r);
    if (f->command_w >= 0) close(f->command_w);
}

static int fixture_open(struct fixture *f)
{
    int commands[2] = {-1, -1};
    struct termios raw;
    memset(f, 0, sizeof(*f));
    f->master = f->slave = f->command_r = f->command_w = -1;
    if (openpty(&f->master, &f->slave, NULL, NULL, NULL) < 0)
        goto fail;
    if (pipe(commands) < 0)
        goto fail;
    f->command_r = commands[0];
    f->command_w = commands[1];
    if (tcgetattr(f->slave, &f->original) < 0)
        goto fail;
    raw = f->original;
    raw.c_iflag &= ~(tcflag_t)(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(tcflag_t)OPOST;
    raw.c_cflag &= ~(tcflag_t)CSIZE;
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(tcflag_t)(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(f->slave, TCSANOW, &raw) < 0)
        goto fail;
    f->raw_enabled = 1;
    return 0;
fail:
    perror("PTY fixture");
    fixture_close(f);
    return 1;
}

static int restore_and_check(struct fixture *f)
{
    struct termios restored;
    if (tcsetattr(f->slave, TCSANOW, &f->original) < 0)
        return 1;
    f->raw_enabled = 0;
    if (tcgetattr(f->slave, &restored) < 0)
        return 1;
    return restored.c_iflag != f->original.c_iflag ||
           restored.c_oflag != f->original.c_oflag ||
           restored.c_cflag != f->original.c_cflag ||
           restored.c_lflag != f->original.c_lflag ||
           memcmp(restored.c_cc, f->original.c_cc,
                  sizeof(restored.c_cc)) != 0;
}

static long elapsed_ms(const struct timespec *a, const struct timespec *b)
{
    return (b->tv_sec - a->tv_sec) * 1000L +
           (b->tv_nsec - a->tv_nsec) / 1000000L;
}

static int test_terminal_prequeued(struct fixture *f)
{
    unsigned ready = 0;
    char c = 0;
    REQUIRE(write(f->master, "A", 1) == 1);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 200, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == IC_READY_TERMINAL);
    REQUIRE(read(f->slave, &c, 1) == 1 && c == 'A');
    return 0;
}

static int test_command_prequeued(struct fixture *f)
{
    unsigned ready = 0;
    char c = 0;
    REQUIRE(write(f->command_w, "C", 1) == 1);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 200, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == IC_READY_COMMAND);
    REQUIRE(read(f->command_r, &c, 1) == 1 && c == 'C');
    return 0;
}

static int test_both_ready(struct fixture *f)
{
    unsigned ready = 0;
    REQUIRE(write(f->master, "T", 1) == 1);
    REQUIRE(write(f->command_w, "P", 1) == 1);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 200, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == (IC_READY_TERMINAL | IC_READY_COMMAND));
    return 0;
}

static int test_timeout(struct fixture *f)
{
    unsigned ready = 123u;
    struct timespec before, after;
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 50, NULL, &ready) == IC_WAIT_TIMEOUT);
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &after) == 0);
    REQUIRE(ready == 0);
    REQUIRE(elapsed_ms(&before, &after) >= 35);
    REQUIRE(elapsed_ms(&before, &after) < 2000);
    return 0;
}

static int test_queued_user_buffer(struct fixture *f)
{
    unsigned ready = 0;
    struct timespec before, after;
    char c = 0;
    REQUIRE(write(f->master, "Q", 1) == 1);
    REQUIRE(read(f->slave, &c, 1) == 1 && c == 'Q');
    /* Byte is now in application memory, not readable on slave fd. */
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 0, NULL, &ready) == IC_WAIT_TIMEOUT);
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
    REQUIRE(ic_wait2(f->slave, f->command_r, 1, 1000, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(clock_gettime(CLOCK_MONOTONIC, &after) == 0);
    REQUIRE(ready == IC_READY_TERMINAL);
    REQUIRE(elapsed_ms(&before, &after) < 150);
    return 0;
}

static int test_buffered_plus_command(struct fixture *f)
{
    unsigned ready = 0;
    REQUIRE(write(f->command_w, "P", 1) == 1);
    REQUIRE(ic_wait2(f->slave, f->command_r, 1, 1000, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == (IC_READY_TERMINAL | IC_READY_COMMAND));
    return 0;
}

static void child_sleep_ms(long milliseconds)
{
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) < 0 && errno == EINTR) { }
}

static int test_delayed_both(struct fixture *f)
{
    unsigned ready = 0;
    int child_status;
    pid_t pid = fork();
    REQUIRE(pid >= 0);
    if (pid == 0) {
        child_sleep_ms(30);
        if (write(f->master, "T", 1) != 1 || write(f->command_w, "C", 1) != 1)
            _exit(1);
        _exit(0);
    }
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 500, NULL, &ready) == IC_WAIT_READY);
    /* The first writer can wake pselect before the other byte arrives.
       Drain the observed event so the second call cannot repeat it. */
    if (ready != (IC_READY_TERMINAL | IC_READY_COMMAND)) {
        unsigned later = 0;
        char consumed = 0;
        if (ready & IC_READY_TERMINAL)
            REQUIRE(read(f->slave, &consumed, 1) == 1 && consumed == 'T');
        else
            REQUIRE(read(f->command_r, &consumed, 1) == 1 && consumed == 'C');
        REQUIRE(ic_wait2(f->slave, f->command_r, 0, 500, NULL, &later) == IC_WAIT_READY);
        ready |= later;
    }
    REQUIRE(waitpid(pid, &child_status, 0) == pid);
    REQUIRE(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
    REQUIRE(ready == (IC_READY_TERMINAL | IC_READY_COMMAND));
    return 0;
}

static int test_pipe_eof(struct fixture *f)
{
    unsigned ready = 0;
    char c;
    REQUIRE(close(f->command_w) == 0);
    f->command_w = -1;
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 100, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == IC_READY_COMMAND);
    REQUIRE(read(f->command_r, &c, 1) == 0);
    return 0;
}

static volatile sig_atomic_t seen_usr1;
static volatile sig_atomic_t seen_winch;
static void signal_seen(int signo)
{
    if (signo == SIGUSR1) seen_usr1++;
    if (signo == SIGWINCH) seen_winch++;
}

/* Block first, queue a signal, then atomically unblock only inside pselect.
 * Unlike sleeping a child for an arbitrary delay, this tests the race
 * deterministically. This function also proves that the wait doesn't eat
 * the application's signal notification. */
static int test_pending_interruption(struct fixture *f)
{
    struct sigaction sa, previous;
    sigset_t blocked, previous_mask, wait_mask;
    unsigned ready = 99;
    int rc, saved_errno;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_seen;
    REQUIRE(sigemptyset(&sa.sa_mask) == 0);
    REQUIRE(sigaction(SIGUSR1, &sa, &previous) == 0);
    REQUIRE(sigemptyset(&blocked) == 0);
    REQUIRE(sigaddset(&blocked, SIGUSR1) == 0);
    REQUIRE(sigprocmask(SIG_BLOCK, &blocked, &previous_mask) == 0);
    seen_usr1 = 0;
    REQUIRE(raise(SIGUSR1) == 0);
    wait_mask = previous_mask;
    REQUIRE(sigdelset(&wait_mask, SIGUSR1) == 0);
    errno = 0;
    rc = ic_wait2(f->slave, f->command_r, 0, 200, &wait_mask, &ready);
    saved_errno = errno;
    REQUIRE(sigprocmask(SIG_SETMASK, &previous_mask, NULL) == 0);
    REQUIRE(sigaction(SIGUSR1, &previous, NULL) == 0);
    REQUIRE(rc == IC_WAIT_INTERRUPTED && saved_errno == EINTR);
    REQUIRE(ready == 0 && seen_usr1 == 1);
    REQUIRE(restore_and_check(f) == 0);
    return 0;
}

static int test_signal_during_wait(struct fixture *f)
{
    struct sigaction sa, previous;
    sigset_t blocked, previous_mask, wait_mask;
    unsigned ready = 99;
    int rc, saved_errno, child_status;
    pid_t parent = getpid(), child;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_seen;
    REQUIRE(sigemptyset(&sa.sa_mask) == 0);
    REQUIRE(sigaction(SIGUSR1, &sa, &previous) == 0);
    REQUIRE(sigemptyset(&blocked) == 0);
    REQUIRE(sigaddset(&blocked, SIGUSR1) == 0);
    REQUIRE(sigprocmask(SIG_BLOCK, &blocked, &previous_mask) == 0);
    seen_usr1 = 0;
    child = fork();
    REQUIRE(child >= 0);
    if (child == 0) {
        child_sleep_ms(35);
        _exit(kill(parent, SIGUSR1) == 0 ? 0 : 1);
    }
    wait_mask = previous_mask;
    REQUIRE(sigdelset(&wait_mask, SIGUSR1) == 0);
    errno = 0;
    rc = ic_wait2(f->slave, f->command_r, 0, 500, &wait_mask, &ready);
    saved_errno = errno;
    REQUIRE(waitpid(child, &child_status, 0) == child);
    REQUIRE(sigprocmask(SIG_SETMASK, &previous_mask, NULL) == 0);
    REQUIRE(sigaction(SIGUSR1, &previous, NULL) == 0);
    REQUIRE(WIFEXITED(child_status) && WEXITSTATUS(child_status) == 0);
    REQUIRE(rc == IC_WAIT_INTERRUPTED && saved_errno == EINTR);
    REQUIRE(ready == 0 && seen_usr1 == 1);
    REQUIRE(restore_and_check(f) == 0);
    return 0;
}

static int test_buffered_wins_over_signal(struct fixture *f)
{
    struct sigaction sa, previous;
    sigset_t blocked, previous_mask, wait_mask;
    unsigned ready = 99;
    int rc;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_seen;
    REQUIRE(sigemptyset(&sa.sa_mask) == 0);
    REQUIRE(sigaction(SIGUSR1, &sa, &previous) == 0);
    REQUIRE(sigemptyset(&blocked) == 0);
    REQUIRE(sigaddset(&blocked, SIGUSR1) == 0);
    REQUIRE(sigprocmask(SIG_BLOCK, &blocked, &previous_mask) == 0);
    seen_usr1 = 0;
    REQUIRE(raise(SIGUSR1) == 0);
    wait_mask = previous_mask;
    REQUIRE(sigdelset(&wait_mask, SIGUSR1) == 0);
    rc = ic_wait2(f->slave, f->command_r, 1, 1000, &wait_mask, &ready);
    REQUIRE(sigprocmask(SIG_SETMASK, &previous_mask, NULL) == 0);
    REQUIRE(sigaction(SIGUSR1, &previous, NULL) == 0);
    REQUIRE(rc == IC_WAIT_READY);
    REQUIRE(ready == IC_READY_TERMINAL && seen_usr1 == 1);
    return 0;
}

static int test_invalid_descriptor(struct fixture *f)
{
    unsigned ready = 99;
    REQUIRE(ic_wait2(-2, f->command_r, 0, 0, NULL, &ready) == IC_WAIT_ERROR);
    REQUIRE(errno == EINVAL && ready == 0);
    REQUIRE(ic_wait2(f->slave, f->slave, 0, 0, NULL, &ready) == IC_WAIT_ERROR);
    REQUIRE(errno == EINVAL && ready == 0);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, -2, NULL, &ready) == IC_WAIT_ERROR);
    REQUIRE(errno == EINVAL && ready == 0);
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 0, NULL, NULL) == IC_WAIT_ERROR);
    REQUIRE(errno == EINVAL);
    REQUIRE(ic_wait2(FD_SETSIZE, f->command_r, 0, 0, NULL, &ready) == IC_WAIT_ERROR);
    REQUIRE(errno == EINVAL && ready == 0);
    REQUIRE(restore_and_check(f) == 0);
    return 0;
}

static int test_closed_descriptor(struct fixture *f)
{
    unsigned ready = 99;
    int stale = f->command_r;
    REQUIRE(close(f->command_r) == 0);
    f->command_r = -1;
    REQUIRE(ic_wait2(f->slave, stale, 0, 0, NULL, &ready) == IC_WAIT_ERROR);
    REQUIRE(errno == EBADF && ready == 0);
    return 0;
}

static int test_no_descriptors(struct fixture *f)
{
    unsigned ready = 99;
    (void)f;
    REQUIRE(ic_wait2(-1, -1, 0, 10, NULL, &ready) == IC_WAIT_TIMEOUT);
    REQUIRE(ready == 0);
    REQUIRE(ic_wait2(-1, -1, 1, 1000, NULL, &ready) == IC_WAIT_READY);
    REQUIRE(ready == IC_READY_TERMINAL);
    return 0;
}

static int test_terminal_restore_normal(struct fixture *f)
{
    unsigned ready = 99;
    REQUIRE(ic_wait2(f->slave, f->command_r, 0, 10, NULL, &ready) == IC_WAIT_TIMEOUT);
    REQUIRE(restore_and_check(f) == 0);
    return 0;
}

static int test_resize_signal(struct fixture *f)
{
#if defined(TIOCSWINSZ) && defined(TIOCGWINSZ)
    struct winsize size, observed;
    struct sigaction sa, previous;
    sigset_t blocked, previous_mask, wait_mask;
    unsigned ready = 99;
    int rc, saved_errno;
    memset(&size, 0, sizeof(size));
    size.ws_row = 31;
    size.ws_col = 83;
    REQUIRE(ioctl(f->master, TIOCSWINSZ, &size) == 0);
    REQUIRE(ioctl(f->slave, TIOCGWINSZ, &observed) == 0);
    REQUIRE(observed.ws_row == 31 && observed.ws_col == 83);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_seen;
    REQUIRE(sigemptyset(&sa.sa_mask) == 0);
    REQUIRE(sigaction(SIGWINCH, &sa, &previous) == 0);
    REQUIRE(sigemptyset(&blocked) == 0);
    REQUIRE(sigaddset(&blocked, SIGWINCH) == 0);
    REQUIRE(sigprocmask(SIG_BLOCK, &blocked, &previous_mask) == 0);
    seen_winch = 0;
    /* No controlling terminal: explicitly queue SIGWINCH, not a claim that
       TIOCSWINSZ generated a foreground-process-group notification. */
    REQUIRE(raise(SIGWINCH) == 0);
    wait_mask = previous_mask;
    REQUIRE(sigdelset(&wait_mask, SIGWINCH) == 0);
    errno = 0;
    rc = ic_wait2(f->slave, f->command_r, 0, 200, &wait_mask, &ready);
    saved_errno = errno;
    REQUIRE(sigprocmask(SIG_SETMASK, &previous_mask, NULL) == 0);
    REQUIRE(sigaction(SIGWINCH, &previous, NULL) == 0);
    REQUIRE(rc == IC_WAIT_INTERRUPTED && saved_errno == EINTR);
    REQUIRE(ready == 0 && seen_winch == 1);
    REQUIRE(restore_and_check(f) == 0);
    return 0;
#else
    (void)f;
    puts("    resize ioctl unsupported; skipped");
    return 0;
#endif
}

struct test_case {
    const char *name;
    int (*run)(struct fixture *);
};

int main(void)
{
    static const struct test_case tests[] = {
        {"terminal ready before wait", test_terminal_prequeued},
        {"command pipe ready before wait", test_command_prequeued},
        {"simultaneous readiness", test_both_ready},
        {"new input arriving during wait", test_delayed_both},
        {"bounded timeout", test_timeout},
        {"user-space buffered terminal input", test_queued_user_buffer},
        {"buffered terminal plus command", test_buffered_plus_command},
        {"pipe EOF is readable", test_pipe_eof},
        {"pending signal: atomic unmask and EINTR", test_pending_interruption},
        {"signal delivered during wait", test_signal_during_wait},
        {"queued bytes remain ready on EINTR", test_buffered_wins_over_signal},
        {"invalid arguments and restore", test_invalid_descriptor},
        {"closed descriptor", test_closed_descriptor},
        {"zero watched descriptors", test_no_descriptors},
        {"normal timeout and terminal restore", test_terminal_restore_normal},
        {"PTY size + SIGWINCH + restore", test_resize_signal}
    };
    size_t i;
    int failures = 0;
    printf("iCurses POSIX input wait regression (%zu cases)\n",
           sizeof(tests)/sizeof(tests[0]));
    for (i = 0; i < sizeof(tests)/sizeof(tests[0]); ++i) {
        struct fixture f;
        int status;
        /* A regression must not leave a CI worker stuck forever. */
        alarm(4);
        if (fixture_open(&f) != 0) return 2;
        status = tests[i].run(&f);
        fixture_close(&f);
        alarm(0);
        printf("%s %s\n", status ? "FAIL" : "PASS", tests[i].name);
        failures += !!status;
    }
    printf("%zu cases: %zu passed, %d failed\n",
           sizeof(tests)/sizeof(tests[0]),
           sizeof(tests)/sizeof(tests[0]) - (size_t)failures,
           failures);
    return failures ? 1 : 0;
}
