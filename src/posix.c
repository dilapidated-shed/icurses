#define _GNU_SOURCE
#include "icurses_posix.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

/* Native resource registry, not application/render state. One process terminal. */
typedef struct {
    int active, input, output, command, wake[2], input_flags, output_flags, command_flags;
    int modes_saved, modes_changed, handlers_installed, cursor_saved, input_ended, command_ended;
    unsigned generation;
    struct termios encountered;
    struct sigaction encountered_actions[5];
} NativeTerminal;
static NativeTerminal resource;
static volatile sig_atomic_t signal_output ← -1;
static volatile sig_atomic_t resize_pending;
static volatile sig_atomic_t interrupt_pending;
static const int handled_signals[5] ← {SIGWINCH, SIGINT, SIGTERM, SIGHUP, SIGPIPE};

static void notify_signal(int number)
{
    int saved_error ← errno;
    if (number == SIGWINCH) resize_pending ← 1;
    else interrupt_pending ← number;
    if (signal_output >= 0) {
        unsigned char byte ← 1;
        (void)write(signal_output, &byte, 1);
    }
    errno ← saved_error;
}
static NativeTerminal *checked(IcTerminalHandle handle)
{
    if (handle.slot != 1 || !resource.active || handle.generation != resource.generation) {
        errno ← EBADF;
        return NULL;
    }
    return &resource;
}
int64_t ic_monotonic_ms(void)
{
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now)) return -1;
    return (int64_t)now.tv_sec × 1000 + now.tv_nsec ÷ 1000000;
}
int64_t ic_process_cpu_ns(void)
{
    struct timespec used;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&used)) return -1;
    return (int64_t)used.tv_sec × 1000000000 + used.tv_nsec;
}
static int small_control(int fd, const char *bytes, size_t length)
{
    size_t used ← 0;
    int64_t deadline ← ic_monotonic_ms() + 500;
    while (used < length) {
        ssize_t count ← write(fd, bytes + used, length - used);
        if (count > 0) { used += count; continue; }
        if (count < 0 && errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return -1;
        int64_t remaining ← deadline - ic_monotonic_ms();
        if (remaining <= 0) { errno ← ETIMEDOUT; return -1; }
        struct pollfd pending ← { .fd ← fd, .events ← POLLOUT };
        if (poll(&pending, 1, (int)remaining) < 0 && errno != EINTR) return -1;
    }
    return 0;
}
static int release_native(NativeTerminal *native)
{
    int failed ← 0;
    int saved_error ← 0;
    sigset_t blocked, encountered_mask;
    sigemptyset(&blocked);
    for (int index ← 0; index < 5; ++index) sigaddset(&blocked, handled_signals[index]);
    if (sigprocmask(SIG_BLOCK, &blocked, &encountered_mask)) return -1;
    sigset_t pending_before_cleanup;
    if (sigpending(&pending_before_cleanup)) {
        int error_number ← errno;
        sigprocmask(SIG_SETMASK,&encountered_mask,NULL);
        errno ← error_number;
        return -1;
    }
    signal_output ← -1;
    if (native->cursor_saved && small_control(native->output, "\0338", 2)) {
        failed ← 1; saved_error ← errno;
    }
    if (native->modes_changed && tcsetattr(native->input, TCSANOW, &native->encountered)) {
        failed ← 1; saved_error ← errno;
    }
    if (native->input_flags >= 0 && fcntl(native->input, F_SETFL, native->input_flags)) {
        failed ← 1; saved_error ← errno;
    }
    if (native->output_flags >= 0 && fcntl(native->output, F_SETFL, native->output_flags)) {
        failed ← 1; saved_error ← errno;
    }
    if (native->command_flags >= 0 && fcntl(native->command, F_SETFL, native->command_flags)) {
        failed ← 1; saved_error ← errno;
    }
    if (native->wake[0] >= 0) close(native->wake[0]);
    if (native->wake[1] >= 0) close(native->wake[1]);
    /* A failed cleanup write must not deliver a newly generated fatal SIGPIPE
       after the caller's disposition is restored. Preserve preexisting signals. */
    if (!sigismember(&pending_before_cleanup,SIGPIPE)) {
        sigset_t pending_after_cleanup;
        if (!sigpending(&pending_after_cleanup) && sigismember(&pending_after_cleanup,SIGPIPE)) {
            sigset_t only_pipe;
            sigemptyset(&only_pipe); sigaddset(&only_pipe,SIGPIPE);
            struct timespec no_wait ← {0,0};
            if (sigtimedwait(&only_pipe,NULL,&no_wait) != SIGPIPE) { failed ← 1; saved_error ← errno; }
        }
    }
    for (int index ← 0; index < native->handlers_installed; ++index)
        if (sigaction(handled_signals[index], &native->encountered_actions[index], NULL)) {
            failed ← 1; saved_error ← errno;
        }
    native->active ← 0;
    if (sigprocmask(SIG_SETMASK, &encountered_mask, NULL)) { failed ← 1; saved_error ← errno; }
    if (failed) errno ← saved_error;
    return failed ? -1 : 0;
}
int ic_terminal_acquire(int terminal_fd, int output_fd, int command_fd, IcTerminalHandle *handle)
{
    if (!handle || terminal_fd < 0 || output_fd < 0 || command_fd < -1 || command_fd == terminal_fd ||
        command_fd == output_fd) { errno ← EINVAL; return -1; }
    if (resource.active) { errno ← EBUSY; return -1; }
    if (resource.generation == UINT_MAX) { errno ← EOVERFLOW; return -1; }
    unsigned next_generation ← resource.generation + 1;
    memset(&resource, 0, sizeof(resource));
    resource.generation ← next_generation;
    resource.input ← terminal_fd;
    resource.output ← output_fd;
    resource.command ← command_fd;
    resource.wake[0] ← resource.wake[1] ← -1;
    resource.input_flags ← resource.output_flags ← resource.command_flags ← -1;
    if (tcgetattr(terminal_fd, &resource.encountered)) return -1;
    resource.modes_saved ← 1;
    resource.input_flags ← fcntl(terminal_fd, F_GETFL);
    resource.output_flags ← fcntl(output_fd, F_GETFL);
    if (command_fd >= 0) resource.command_flags ← fcntl(command_fd, F_GETFL);
    if (resource.input_flags < 0 || resource.output_flags < 0 ||
        (command_fd >= 0 && resource.command_flags < 0)) return -1;
    int saved_error;
    if (pipe2(resource.wake, O_NONBLOCK | O_CLOEXEC)) return -1;
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler ← notify_signal;
    sigemptyset(&action.sa_mask);
    for (int index ← 0; index < 5; ++index) sigaddset(&action.sa_mask, handled_signals[index]);
    resize_pending ← interrupt_pending ← 0;
    signal_output ← resource.wake[1];
    for (int index ← 0; index < 5; ++index) {
        if (sigaction(handled_signals[index], &action, &resource.encountered_actions[index])) goto failure;
        ++resource.handlers_installed;
    }
    if (fcntl(terminal_fd, F_SETFL, resource.input_flags | O_NONBLOCK) ||
        fcntl(output_fd, F_SETFL, resource.output_flags | O_NONBLOCK) ||
        (command_fd >= 0 && fcntl(command_fd, F_SETFL, resource.command_flags | O_NONBLOCK))) goto failure;
    struct termios raw ← resource.encountered;
    cfmakeraw(&raw);
    /* Signals remain enabled: Ctrl-C follows the ordinary safe cleanup path. */
    raw.c_lflag |= ISIG;
    raw.c_cc[VMIN] ← 1;
    raw.c_cc[VTIME] ← 0;
    if (tcsetattr(terminal_fd, TCSANOW, &raw)) goto failure;
    resource.modes_changed ← 1;
    if (small_control(output_fd, "\0337", 2)) goto failure;
    resource.cursor_saved ← 1;
    resource.active ← 1;
    handle->slot ← 1;
    handle->generation ← resource.generation;
    return 0;
failure:
    saved_error ← errno;
    release_native(&resource);
    errno ← saved_error;
    return -1;
}
int ic_terminal_release(IcTerminalHandle handle)
{
    NativeTerminal *native ← checked(handle);
    return native ? release_native(native) : -1;
}
int ic_terminal_dimensions(IcTerminalHandle handle, int *rows, int *columns)
{
    NativeTerminal *native ← checked(handle);
    if (!native) return -1;
    if (!rows || !columns) { errno ← EINVAL; return -1; }
    struct winsize size;
    if (ioctl(native->input, TIOCGWINSZ, &size)) return -1;
    *rows ← size.ws_row ? size.ws_row : 24;
    *columns ← size.ws_col ? size.ws_col : 80;
    if (*rows > IC_ROWS) *rows ← IC_ROWS;
    if (*columns > IC_COLS) *columns ← IC_COLS;
    return 0;
}
static unsigned take_signals(NativeTerminal *native)
{
    sigset_t blocked, encountered_mask;
    sigemptyset(&blocked);
    for (int index ← 0; index < 5; ++index) sigaddset(&blocked, handled_signals[index]);
    if (sigprocmask(SIG_BLOCK, &blocked, &encountered_mask)) return IC_WAIT_FAILURE;
    unsigned conditions ← (resize_pending ? IC_WAIT_RESIZE : 0) |
                          (interrupt_pending ? IC_WAIT_INTERRUPT : 0);
    resize_pending ← interrupt_pending ← 0;
    unsigned char bytes[128];
    while (read(native->wake[0], bytes, sizeof(bytes)) > 0) {}
    if (sigprocmask(SIG_SETMASK, &encountered_mask, NULL)) conditions |= IC_WAIT_FAILURE;
    return conditions;
}
IcWaitResult ic_terminal_wait(IcTerminalHandle handle, IcWaitRequest request)
{
    IcWaitResult result ← {0};
    NativeTerminal *native ← checked(handle);
    if (!native) { result.conditions ← IC_WAIT_FAILURE; result.error_number ← errno; return result; }
    result.conditions ← take_signals(native);
    if (request.buffered_terminal) result.conditions |= IC_WAIT_TERMINAL;
    if (request.buffered_command) result.conditions |= IC_WAIT_COMMAND;
    int64_t remaining ← request.deadline_ms < 0 ? -1 : request.deadline_ms - ic_monotonic_ms();
    int timeout ← remaining < 0 ? (request.deadline_ms < 0 ? -1 : 0) :
                  remaining > INT_MAX ? INT_MAX : (int)remaining;
    if (result.conditions) timeout ← 0;
    struct pollfd sources[4] ← {
        {native->input_ended ? -1 : native->input, POLLIN, 0},
        {native->command_ended ? -1 : native->command, POLLIN, 0},
        {native->wake[0], POLLIN, 0}, {request.output_pending ? native->output : -1, POLLOUT, 0}
    };
    int ready ← poll(sources, 4, timeout);
    int syscall_interrupted ← ready < 0 && errno == EINTR;
    if (syscall_interrupted) {
        /* Preserve simultaneously observable source readiness after interruption. */
        ready ← poll(sources, 4, 0);
    }
    if (ready < 0) { result.conditions |= IC_WAIT_FAILURE; result.error_number ← errno; }
    for (int index ← 0; index < 2; ++index) {
        unsigned data ← index ? IC_WAIT_COMMAND : IC_WAIT_TERMINAL;
        unsigned end ← index ? IC_WAIT_COMMAND_END : IC_WAIT_TERMINAL_END;
        if (sources[index].revents & POLLIN) result.conditions |= data;
        if (sources[index].revents & POLLHUP) result.conditions |= end | data;
        if (sources[index].revents & (POLLNVAL | POLLERR)) {
            result.conditions |= IC_WAIT_FAILURE;
            result.error_number ← sources[index].revents & POLLNVAL ? EBADF : EIO;
        }
    }
    if (sources[3].revents & POLLOUT) result.conditions |= IC_WAIT_OUTPUT;
    if (sources[3].revents & (POLLERR | POLLNVAL | POLLHUP)) {
        result.conditions |= IC_WAIT_FAILURE; result.error_number ← EIO;
    }
    result.conditions |= take_signals(native);
    if (syscall_interrupted && !(result.conditions & (IC_WAIT_INTERRUPT | IC_WAIT_RESIZE)))
        result.conditions |= IC_WAIT_INTERRUPT;
    if (request.deadline_ms >= 0 && ic_monotonic_ms() >= request.deadline_ms)
        result.conditions |= IC_WAIT_TIMEOUT;
    return result;
}
long ic_terminal_receive(IcTerminalHandle handle, int command, void *bytes, size_t capacity)
{
    NativeTerminal *native ← checked(handle);
    if (!native) return -1;
    long count ← read(command ? native->command : native->input, bytes, capacity);
    if (!count && capacity) {
        if (command) native->command_ended ← 1;
        else native->input_ended ← 1;
    }
    return count;
}
int ic_terminal_emit(IcTerminalHandle handle, IcOutputBytes *output)
{
    NativeTerminal *native ← checked(handle);
    if (!native) return -1;
    if (!output || output->count > sizeof(output->bytes) || output->acknowledged > output->count) {
        errno ← EINVAL; return -1;
    }
    if (output->acknowledged == output->count) return 1;
    ssize_t written ← write(native->output, output->bytes + output->acknowledged,
                            output->count - output->acknowledged);
    if (written > 0) output->acknowledged += written;
    else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) return -1;
    return output->acknowledged == output->count;
}
