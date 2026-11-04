# Declared terminal and text limits

The supported profile is an xterm-compatible UTF-8 terminal with CUP, ANSI
eight-color SGR and DEC save/restore cursor plus rendition. The implementation
does not negotiate terminfo or claim ncurses API compatibility. Initial cursor
position and rendition are saved with DECSC and restored with DECRC; it does
not assume the original cursor position or color. It does not change alternate
screen or cursor-visibility modes. Drawn text remains on screen after exit.

The viewport is capped at 64 rows by 200 columns; larger terminals retain their
application data and use the upper-left bounded viewport. Resizing invalidates
cells and preserves Lua application/selection state. Screens, regions, decoder
buffers and output queues have fixed declared capacities. A full-screen output
transaction cannot be superseded until all of its bytes are acknowledged.
On unrecoverable output failure, the application exits and attempts restoration.

Width policy is locale-independent: the ranges listed in `ic_codepoint_columns`
are zero-width combining/variation ranges or two-column East Asian ranges;
other printable scalar values occupy one column. C0, DEL, and C1 text are
replaced with U+FFFD on the literal-text path. Invalid UTF-8 is replaced and
incomplete incremental input remains pending. A cell carries at most twenty
UTF-8 bytes. Excess combining bytes are dropped deterministically. An orphan
combining mark is dropped. A wide glyph clipped at either edge is omitted as a
whole. Overwriting either half erases the previous wide occupant.

This is not universal Unicode grapheme layout. Emoji width, ZWJ sequences,
regional-indicator flags, bidirectional shaping, fonts, and full canonical
equivalence are unsupported. The terminal must agree with the declared width
policy for the characters displayed. Turkish scalar letters, a base plus a
combining acute accent, and U+754C are explicit fixtures, not universal claims.

The incremental key decoder supports UTF-8 text, Escape, common CSI/SS3 arrows,
Home/End and Page Up/Down. Mouse, bracketed paste, modifier combinations and
arbitrary terminal reports are outside this profile. A partial escape sequence
has a runner-owned explicit 35 ms ambiguity deadline; the pure decoder accepts
an expiry decision and never reads or sleeps. UTF-8 partial sequences have no
implicit read or escape timeout.

Normal exit, initialization failure, recoverable error, SIGINT, SIGTERM, SIGHUP
and a broken output pipe use ordinary cleanup outside handlers. The handler
only sets signal-safe flags and writes one wake byte. Signal dispositions,
descriptor flags and the actual encountered termios are restored. The adapter
requires single-threaded signal ownership while active. SIGTSTP/job-control
suspension is not implemented. SIGKILL, catastrophic memory corruption,
unavailable/closed borrowed resources, power loss and a terminal that cannot
accept restoration output cannot be repaired by this process. Restoration
failures are returned rather than treated as success.
