#define _GNU_SOURCE
#include "icurses_posix.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define REQUIRE(condition) do { if (!(condition)) { fprintf(stderr,"FAIL %s:%d %s: %s (%d)\n",__FILE__,__LINE__,#condition,strerror(errno),errno); exit(1); } } while (0)
typedef struct { int master, slave, pipe[2], flags; struct termios encountered; IcTerminalHandle handle; } Fixture;
static void open_fixture(Fixture *fixture)
{
    struct winsize size ← { .ws_row ← 24, .ws_col ← 80 };
    REQUIRE(!openpty(&fixture->master, &fixture->slave, NULL, NULL, &size));
    REQUIRE(!pipe(fixture->pipe));
    REQUIRE(!tcgetattr(fixture->slave, &fixture->encountered));
    fixture->flags ← fcntl(fixture->slave, F_GETFL);
    REQUIRE(!ic_terminal_acquire(fixture->slave, fixture->slave, fixture->pipe[0], &fixture->handle));
}
static int same_modes(const struct termios *first, const struct termios *second)
{
    return first->c_iflag == second->c_iflag && first->c_oflag == second->c_oflag &&
        first->c_cflag == second->c_cflag && first->c_lflag == second->c_lflag &&
        !memcmp(first->c_cc, second->c_cc, sizeof(first->c_cc)) &&
        cfgetispeed(first) == cfgetispeed(second) && cfgetospeed(first) == cfgetospeed(second);
}
static void close_fixture(Fixture *fixture)
{
    REQUIRE(!ic_terminal_release(fixture->handle));
    struct termios restored;
    REQUIRE(!tcgetattr(fixture->slave, &restored));
    REQUIRE(same_modes(&fixture->encountered, &restored));
    REQUIRE(fcntl(fixture->slave, F_GETFL) == fixture->flags);
    close(fixture->master); close(fixture->slave);
    close(fixture->pipe[0]);
    if (fixture->pipe[1] >= 0) close(fixture->pipe[1]);
}
static IcWaitResult waiting(Fixture *fixture, int milliseconds, int buffered)
{
    IcWaitRequest request ← {ic_monotonic_ms() + milliseconds, buffered, 0, 0};
    return ic_terminal_wait(fixture->handle, request);
}
static void readiness(void)
{
    Fixture fixture;
    open_fixture(&fixture);
    char byte;
    REQUIRE(write(fixture.master, "a", 1) == 1);
    IcWaitResult result ← waiting(&fixture, 500, 0);
    REQUIRE(result.conditions & IC_WAIT_TERMINAL);
    REQUIRE(ic_terminal_receive(fixture.handle, 0, &byte, 1) == 1 && byte == 'a');
    REQUIRE(write(fixture.pipe[1], "b", 1) == 1);
    result ← waiting(&fixture, 500, 0);
    REQUIRE(result.conditions & IC_WAIT_COMMAND);
    REQUIRE(ic_terminal_receive(fixture.handle, 1, &byte, 1) == 1 && byte == 'b');
    REQUIRE(write(fixture.master, "c", 1) == 1 && write(fixture.pipe[1], "d", 1) == 1);
    /* PTY delivery can be asynchronous. Establish kernel readiness before simultaneity. */
    struct pollfd injected ← {fixture.slave, POLLIN, 0};
    REQUIRE(poll(&injected, 1, 500) == 1);
    result ← waiting(&fixture, 500, 0);
    REQUIRE((result.conditions & (IC_WAIT_TERMINAL | IC_WAIT_COMMAND)) ==
            (IC_WAIT_TERMINAL | IC_WAIT_COMMAND));
    REQUIRE(ic_terminal_receive(fixture.handle, 0, &byte, 1) == 1);
    REQUIRE(ic_terminal_receive(fixture.handle, 1, &byte, 1) == 1);
    int64_t start ← ic_monotonic_ms();
    result ← waiting(&fixture, 500, 1);
    REQUIRE(result.conditions & IC_WAIT_TERMINAL);
    REQUIRE(ic_monotonic_ms() - start < 200);
    REQUIRE(write(fixture.pipe[1], "e", 1) == 1);
    result ← waiting(&fixture, 500, 1);
    REQUIRE((result.conditions & (IC_WAIT_TERMINAL | IC_WAIT_COMMAND)) ==
            (IC_WAIT_TERMINAL | IC_WAIT_COMMAND));
    REQUIRE(ic_terminal_receive(fixture.handle, 1, &byte, 1) == 1);
    close(fixture.pipe[1]); fixture.pipe[1] ← -1;
    result ← waiting(&fixture, 500, 0);
    REQUIRE((result.conditions & (IC_WAIT_COMMAND | IC_WAIT_COMMAND_END)) ==
            (IC_WAIT_COMMAND | IC_WAIT_COMMAND_END));
    REQUIRE(ic_terminal_receive(fixture.handle, 1, &byte, 1) == 0);
    result ← waiting(&fixture, 30, 0);
    REQUIRE(result.conditions & IC_WAIT_TIMEOUT);
    close_fixture(&fixture);
    result ← ic_terminal_wait(fixture.handle, (IcWaitRequest){0,0,0,0});
    REQUIRE((result.conditions & IC_WAIT_FAILURE) && result.error_number == EBADF);
    puts("PASS prequeued, simultaneous, buffered, pipe-end, stale-handle readiness");
}
static void pause_ms(int milliseconds)
{
    struct timespec delay ← {milliseconds ÷ 1000, (milliseconds % 1000) × 1000000};
    while (nanosleep(&delay, &delay) && errno == EINTR) {}
}
static pid_t wait_child(pid_t process, int *status)
{
    pid_t waited;
    do { waited ← waitpid(process,status,0); } while (waited < 0 && errno == EINTR);
    return waited;
}
static void delayed_and_interrupted(void)
{
    Fixture fixture;
    open_fixture(&fixture);
    pid_t parent ← getpid();
    pid_t producer ← fork();
    REQUIRE(producer >= 0);
    if (!producer) {
        pause_ms(25);
        if (write(fixture.master, "z", 1) != 1) _exit(2);
        pause_ms(25);
        if (kill(parent, SIGWINCH)) _exit(3);
        _exit(0);
    }
    IcWaitResult result ← waiting(&fixture, 500, 0);
    REQUIRE(result.conditions & IC_WAIT_TERMINAL);
    char byte;
    REQUIRE(ic_terminal_receive(fixture.handle, 0, &byte, 1) == 1 && byte == 'z');
    result ← waiting(&fixture, 500, 0);
    REQUIRE(result.conditions & IC_WAIT_RESIZE);
    REQUIRE(!(result.conditions & IC_WAIT_INTERRUPT));
    int status;
    REQUIRE(wait_child(producer, &status) == producer && WIFEXITED(status) && !WEXITSTATUS(status));
    producer ← fork();
    REQUIRE(producer >= 0);
    if (!producer) {
        for (int index ← 0; index < 40; ++index) { pause_ms(3); kill(parent, SIGWINCH); }
        _exit(0);
    }
    int64_t start ← ic_monotonic_ms();
    int64_t deadline ← start + 80;
    do {
        result ← ic_terminal_wait(fixture.handle, (IcWaitRequest){deadline,0,0,0});
    } while (!(result.conditions & IC_WAIT_TIMEOUT));
    REQUIRE(ic_monotonic_ms() - start >= 75 && ic_monotonic_ms() - start < 400);
    REQUIRE(wait_child(producer, &status) == producer);
    REQUIRE(!kill(getpid(), SIGINT));
    result ← waiting(&fixture, 500, 1);
    REQUIRE((result.conditions & (IC_WAIT_TERMINAL | IC_WAIT_INTERRUPT)) ==
            (IC_WAIT_TERMINAL | IC_WAIT_INTERRUPT));
    close_fixture(&fixture);
    puts("PASS delayed input, interruption information, bounded absolute deadline");
}
static void sustained(void)
{
    Fixture fixture;
    open_fixture(&fixture);
    for (int index ← 0; index < 100; ++index) {
        REQUIRE(write(fixture.master, "activity", 8) == 8);
        REQUIRE(write(fixture.pipe[1], "u", 1) == 1);
        IcWaitResult result ← waiting(&fixture, 300, 1);
        REQUIRE(result.conditions & IC_WAIT_COMMAND);
        char bytes[32];
        REQUIRE(ic_terminal_receive(fixture.handle, 1, bytes, sizeof(bytes)) == 1);
        (void)ic_terminal_receive(fixture.handle, 0, bytes, sizeof(bytes));
    }
    close_fixture(&fixture);
    puts("PASS sustained terminal activity does not hide command updates");
}
static void failed_initialization(void)
{
    int master, slave;
    REQUIRE(!openpty(&master, &slave, NULL, NULL, NULL));
    struct termios encountered, restored;
    REQUIRE(!tcgetattr(slave, &encountered));
    int full ← open("/dev/full", O_WRONLY);
    REQUIRE(full >= 0);
    IcTerminalHandle handle;
    REQUIRE(ic_terminal_acquire(slave, full, -1, &handle) == -1);
    REQUIRE(!tcgetattr(slave, &restored));
    REQUIRE(same_modes(&encountered, &restored));
    REQUIRE(ic_terminal_acquire(slave, -1, -1, &handle) == -1);
    REQUIRE(!ic_terminal_acquire(slave, slave, -1, &handle));
    REQUIRE(!ic_terminal_release(handle));
    close(full); close(master); close(slave);
    Fixture fixture;
    open_fixture(&fixture);
    close(fixture.pipe[0]);
    IcWaitResult result ← waiting(&fixture, 200, 1);
    REQUIRE((result.conditions & (IC_WAIT_FAILURE | IC_WAIT_TERMINAL)) == (IC_WAIT_FAILURE | IC_WAIT_TERMINAL));
    REQUIRE(result.error_number == EBADF);
    /* Restore the externally destroyed borrowed descriptor before cleanup. */
    int replacement[2];
    REQUIRE(!pipe(replacement));
    REQUIRE(replacement[0] == fixture.pipe[0]);
    close(replacement[1]);
    close_fixture(&fixture);
    puts("PASS partial initialization failure, invalid source, borrowed descriptors");
}
static IcOutputBytes pending;
static IcScreen previous, target;
static void backpressure(void)
{
    Fixture fixture;
    open_fixture(&fixture);
    REQUIRE(fcntl(fixture.master, F_SETFL, O_NONBLOCK) == 0);
    memset(pending.bytes, 'X', sizeof(pending.bytes));
    pending.count ← sizeof(pending.bytes);
    REQUIRE(!ic_empty_screen(24,80,&target));
    REQUIRE(ic_terminal_emit(fixture.handle, &pending) == 0);
    REQUIRE(pending.acknowledged > 0 && pending.acknowledged < pending.count);
    REQUIRE(!ic_acknowledged_output_to_previous(&pending,&target,&previous) && previous.rows == 0);
    size_t acknowledged ← pending.acknowledged;
    for (int index ← 0; index < 1000 && pending.acknowledged < pending.count; ++index) {
        char drain[32768];
        while (read(fixture.master, drain, sizeof(drain)) > 0) {}
        IcWaitResult result ← ic_terminal_wait(fixture.handle,
            (IcWaitRequest){ic_monotonic_ms()+1000,0,0,1});
        REQUIRE(result.conditions & IC_WAIT_OUTPUT);
        REQUIRE(ic_terminal_emit(fixture.handle,&pending) >= 0);
        REQUIRE(pending.acknowledged >= acknowledged);
        acknowledged ← pending.acknowledged;
    }
    REQUIRE(pending.acknowledged == pending.count);
    REQUIRE(ic_acknowledged_output_to_previous(&pending,&target,&previous) && previous.rows == 24);
    char drain[32768];
    while (read(fixture.master, drain, sizeof(drain)) > 0) {}
    close_fixture(&fixture);
    puts("PASS real PTY output backpressure, partial acknowledgement and retry");
}
static void real_resize(void)
{
    int master, slave, handshake[2];
    struct winsize initial ← { .ws_row ← 24, .ws_col ← 80 };
    REQUIRE(!openpty(&master, &slave, NULL, NULL, &initial));
    REQUIRE(!pipe(handshake));
    struct termios encountered, restored;
    REQUIRE(!tcgetattr(slave, &encountered));
    pid_t terminal_process ← fork();
    REQUIRE(terminal_process >= 0);
    if (!terminal_process) {
        close(master); close(handshake[0]);
        REQUIRE(setsid() == getpid());
        REQUIRE(!ioctl(slave, TIOCSCTTY, 0));
        REQUIRE(!tcsetpgrp(slave, getpgrp()));
        REQUIRE(tcgetpgrp(slave) == getpgrp());
        IcTerminalHandle handle;
        REQUIRE(!ic_terminal_acquire(slave, slave, -1, &handle));
        REQUIRE(write(handshake[1], "R", 1) == 1);
        IcWaitResult result ← ic_terminal_wait(handle,(IcWaitRequest){ic_monotonic_ms()+2000,0,0,0});
        REQUIRE(result.conditions & IC_WAIT_RESIZE);
        REQUIRE(!(result.conditions & IC_WAIT_INTERRUPT));
        int rows, columns;
        REQUIRE(!ic_terminal_dimensions(handle,&rows,&columns) && rows == 31 && columns == 97);
        REQUIRE(!ic_terminal_release(handle));
        _exit(0);
    }
    close(handshake[1]);
    char ready;
    REQUIRE(read(handshake[0], &ready, 1) == 1 && ready == 'R');
    struct winsize resized ← { .ws_row ← 31, .ws_col ← 97 };
    /* No kill/raise here: the PTY kernel must notify its foreground process. */
    REQUIRE(!ioctl(master, TIOCSWINSZ, &resized));
    int status;
    REQUIRE(waitpid(terminal_process,&status,0) == terminal_process && WIFEXITED(status) && !WEXITSTATUS(status));
    REQUIRE(!tcgetattr(slave,&restored) && same_modes(&encountered,&restored));
    close(master); close(slave); close(handshake[0]);
    puts("PASS genuine TIOCSWINSZ foreground-session resize and restoration");
}
int main(int argc, char **argv)
{
    alarm(20);
    if (argc == 2 && !strcmp(argv[1],"readiness")) { readiness(); return 0; }
    if (argc == 2 && !strcmp(argv[1],"restoration")) { failed_initialization(); return 0; }
    readiness(); delayed_and_interrupted(); sustained(); failed_initialization(); backpressure(); real_resize();
    return 0;
}
