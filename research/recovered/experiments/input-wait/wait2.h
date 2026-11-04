#ifndef ICURSES_EXPERIMENT_WAIT2_H
#define ICURSES_EXPERIMENT_WAIT2_H

#include <signal.h>

/* A POSIX-only, two-descriptor experiment; NOT a curses API. */
enum ic_wait_result {
    IC_WAIT_ERROR = -2,       /* inspect errno */
    IC_WAIT_INTERRUPTED = -1, /* errno == EINTR; caller handles signal flags */
    IC_WAIT_TIMEOUT = 0,
    IC_WAIT_READY = 1
};

enum ic_wait_ready {
    IC_READY_TERMINAL = 1u << 0,
    IC_READY_COMMAND  = 1u << 1
};

/*
 * - terminal_fd/command_fd: readable POSIX descriptors; -1 disables a source.
 * - buffered_terminal: caller already has decoded/queued terminal input.
 *   That takes precedence over blocking; still poll the other fd immediately.
 * - timeout_ms: -1 waits indefinitely; zero polls; positive bounds the wait.
 * - during_wait: optional signal mask atomically installed by pselect().
 *   To avoid a SIGWINCH race, block it before checking the resize flag,
 *   then pass a mask with SIGWINCH unblocked here.
 * - ready is cleared on entry and contains IC_READY_* when result is READY.
 *   Readable includes EOF/hangup: actual read() must distinguish data/EOF.
 *   This function does not read bytes, decode keys, or change terminal modes.
 *
 * Does not retry EINTR: the caller must handle the signal, then recompute
 * its remaining timeout if it chooses to wait again.
 */
int ic_wait2(int terminal_fd, int command_fd, int buffered_terminal,
             int timeout_ms, const sigset_t *during_wait, unsigned *ready);

#endif
