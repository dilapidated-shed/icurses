#define _POSIX_C_SOURCE 200809L
#include "wait2.h"

#include <errno.h>
#include <stddef.h>
#include <sys/select.h>
#include <time.h>

int ic_wait2(int terminal_fd, int command_fd, int buffered_terminal,
             int timeout_ms, const sigset_t *during_wait, unsigned *ready)
{
    fd_set readers;
    struct timespec limit;
    const struct timespec *limit_ptr = NULL;
    int maxfd = -1;
    int rc;
    unsigned found = buffered_terminal ? IC_READY_TERMINAL : 0u;

    if (!ready) {
        errno = EINVAL;
        return IC_WAIT_ERROR;
    }
    *ready = 0;
    if (terminal_fd < -1 || command_fd < -1 || timeout_ms < -1 ||
        (terminal_fd >= 0 && terminal_fd == command_fd) ||
        terminal_fd >= FD_SETSIZE || command_fd >= FD_SETSIZE) {
        errno = EINVAL;
        return IC_WAIT_ERROR;
    }

    FD_ZERO(&readers);
    if (terminal_fd >= 0) {
        FD_SET(terminal_fd, &readers);
        maxfd = terminal_fd;
    }
    if (command_fd >= 0) {
        FD_SET(command_fd, &readers);
        if (command_fd > maxfd)
            maxfd = command_fd;
    }

    /* A user-space queued byte makes even an infinite timeout nonblocking. */
    if (buffered_terminal)
        timeout_ms = 0;
    if (timeout_ms >= 0) {
        limit.tv_sec = timeout_ms / 1000;
        limit.tv_nsec = (long)(timeout_ms % 1000) * 1000000L;
        limit_ptr = &limit;
    }

    rc = pselect(maxfd + 1, &readers, NULL, NULL, limit_ptr, during_wait);
    if (rc < 0) {
        if (errno == EINTR) {
            if (found) {
                *ready = found;
                return IC_WAIT_READY;
            }
            return IC_WAIT_INTERRUPTED;
        }
        return IC_WAIT_ERROR;
    }
    if (rc > 0) {
        if (terminal_fd >= 0 && FD_ISSET(terminal_fd, &readers))
            found |= IC_READY_TERMINAL;
        if (command_fd >= 0 && FD_ISSET(command_fd, &readers))
            found |= IC_READY_COMMAND;
    }
    *ready = found;
    return found ? IC_WAIT_READY : IC_WAIT_TIMEOUT;
}
