# POSIX terminal + command-pipe wait experiment

**Status:** isolated local experiment; no imported sources were modified.
**Source baseline:** `dilapidated-shed/icurses` `main` at
`663ca05acbab55cf82406d21acf792f4a9b98c6b` (4 November 2026).

This is a deliberately small wait primitive, **not a curses replacement**.
It does not depend on or link radare2, PDCurses, or ncurses.

## Build and run

```sh
cd experiments/input-wait
make test                      # POSIX host with openpty(); Linux needs -lutil
make clean test CC=clang       # alternative compiler
```

The test harness opens a fresh pseudoterminal and pipe for each case, switches
only the PTY slave to noncanonical/no-echo mode, and restores the saved `termios`
state. Each case is guarded by a 4-second alarm against deadlock. Tests inspect
both sources without requiring a real interactive terminal. On Linux,
`<pty.h>` and `-lutil` are used; macOS and BSD use `<util.h>` (not yet tested).

`wait2.c` exposes precisely one nonblocking/waiting operation:

```c
int ic_wait2(int terminal_fd, int command_fd, int buffered_terminal,
             int timeout_ms, const sigset_t *during_wait, unsigned *ready);
```

- Readiness is bitwise (`IC_READY_TERMINAL`, `IC_READY_COMMAND`).
- Already-buffered terminal input forces a zero-duration OS wait, but the
  command descriptor is still checked so both events can be returned.
- `-1` disables a descriptor; a timeout of `-1` means indefinite wait.
- Results are `READY`, `TIMEOUT`, `INTERRUPTED` (`errno=EINTR`), or `ERROR`
  (`errno` supplied). An interrupt does not erase known buffered input.
- `pselect` atomically substitutes `during_wait` if passed. For SIGWINCH,
  callers must block that signal before checking their resize flag, then
  allow it in the wait mask. Handler work should remain signal-safe.
- `POLLHUP`-style distinctions are intentionally **not** exposed: on `select`
  a pipe at EOF is readable; `read()` distinguishes data from EOF.
- Invalid descriptors and descriptor values `>= FD_SETSIZE` are rejected.
  This is a two-fd prototype, not a general fd multiplexer. An `EINTR`
  does **not** automatically restart; the caller recomputes remaining time.
- The waiting function never owns terminal state and does not decode keys.

## Regression cases

1. PTY input ready before entering the wait
2. Command pipe ready before entering the wait
3. Simultaneous PTY and pipe readiness
4. Input arriving after entering the wait
5. Finite timeout (monotonic elapsed time checked)
6. Input already moved from kernel into an application's byte buffer
7. Buffered terminal input plus a ready command pipe
8. Pipe writer closed: read readiness followed by EOF
9. Pending SIGUSR1 delivered by atomic unmask within `pselect()`
10. SIGUSR1 delivered by a child during the wait
11. Pending signal does not hide an already-buffered terminal byte
12. Invalid parameters / FD_SETSIZE boundary and post-error restoration
13. Closed descriptor returns `EBADF`
14. No watched descriptors (timeout or buffered input)
15. Terminal restoration after normal timeout
16. PTY window size change, simulated SIGWINCH, and restoration

The `SIGWINCH` test **explicitly raises** the signal. It verifies PTY size
via `TIOCGWINSZ`, but does not claim that the PTY ioctl delivered a kernel
resize notification to a controlling terminal's foreground process group.
The harness cannot prove restoration after SIGKILL or process crash.

## Source comparison (inspection, not backend execution)

The pinned radare2 [`libr_cons/input.c`](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/libr_cons/input.c#L683-L702)
implements `r_cons_readchar_timeout()` by selecting **fd 0 first**, then
calling `r_cons_readchar()`. The latter tests `InputState.readbuffer_length`
**before** its own `pselect()` wait
([`input.c` lines 733–794](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/libr_cons/input.c#L733-L794)).
Consequently, if its application buffer has bytes but fd 0 does not, the
timeout wrapper can wait unnecessarily. `r_cons_readchar()` itself uses a
SIGWINCH-unmasking `pselect` strategy on Unix. Neither function accepts a
separate application command-pipe descriptor. This is a source-code
observation; the imported radare2 files were not linked or executed.

PDCurses' imported [`curses.h` lines 1415–1419](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/pdcurses/curses.h#L1415-L1419)
marks `PDC_get_input_fd()` deprecated and defines it to expand to **0**.
That macro is not a portable platform input descriptor. PDCurses' actual
[`wgetch()` loop](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/pdcurses/pdcurses/getch.c#L325-L401)
uses backend `PDC_check_key()` with 50-ms `napms()` steps. In the
[X11 backend](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/pdcurses/x11/pdckbd.c#L462-L486),
readiness depends on `XtAppPending()` and `XtAppNextEvent()`. The
[SDL2 backend](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/pdcurses/sdl2/pdckbd.c#L80-L97)
uses `SDL_PollEvent()`, and the
[Windows console backend](https://github.com/dilapidated-shed/icurses/blob/663ca05acbab55cf82406d21acf792f4a9b98c6b/pdcurses/wincon/pdckbd.c#L257-L267)
uses `GetNumberOfConsoleInputEvents()`. The Windows implementation also has
its own key-repeat buffering. None of these backends was built or run here.

**Candidate for reuse:** on POSIX TTY applications, the only reusable
primitive established by this experiment is `ic_wait2()` plus a caller-owned
input queue and a signal flag. It is not suitable as a universal PDCurses
backend abstraction. It intentionally has no registration system, keymap,
renderer, timer framework, or ncurses compatibility surface.

**Verification environment:** Linux x86_64, GCC 14.2 and Clang (recorded
separately in local test logs), using isolated PTYs. No physical Android,
Windows, macOS, X11, or SDL2 acceptance has been performed.
