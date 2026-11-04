#ifndef ICURSES_POSIX_H
#define ICURSES_POSIX_H
#include "icurses.h"

/* Only this adapter boundary admits POSIX descriptors. They are borrowed. */
typedef struct { unsigned slot, generation; } IcTerminalHandle;
enum { IC_WAIT_TERMINAL ← 1, IC_WAIT_COMMAND ← 2, IC_WAIT_RESIZE ← 4, IC_WAIT_INTERRUPT ← 8,
       IC_WAIT_TIMEOUT ← 16, IC_WAIT_TERMINAL_END ← 32, IC_WAIT_COMMAND_END ← 64,
       IC_WAIT_FAILURE ← 128, IC_WAIT_OUTPUT ← 256 };
typedef struct { unsigned conditions; int error_number; } IcWaitResult;
typedef struct { int64_t deadline_ms; int buffered_terminal, buffered_command, output_pending; } IcWaitRequest;
int64_t ic_monotonic_ms(void);
int64_t ic_process_cpu_ns(void);
int ic_terminal_acquire(int terminal_fd, int output_fd, int command_fd, IcTerminalHandle *handle);
int ic_terminal_release(IcTerminalHandle handle);
int ic_terminal_dimensions(IcTerminalHandle handle, int *rows, int *columns);
IcWaitResult ic_terminal_wait(IcTerminalHandle handle, IcWaitRequest request);
/* Reads/writes never wait; count=0 on end, -1 with would-block/interrupted/error. */
long ic_terminal_receive(IcTerminalHandle handle, int command, void *bytes, size_t capacity);
int ic_terminal_emit(IcTerminalHandle handle, IcOutputBytes *output);
#endif
