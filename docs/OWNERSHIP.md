# iCurses ownership and transformation boundary

The public semantic layer is `include/icurses.h`. It contains bounded values,
not descriptors, platform headers, a hidden application engine, or a key map.

| Input | Transformation | Output and owner |
| --- | --- | --- |
| Supplied bytes and explicit escape-expiry decision | `ic_received_bytes_to_event` | Event and remaining bytes; caller owns both |
| Literal text, position, style, desired cells | `ic_text_and_placement` | Desired cells; presentation caller owns storage |
| Previous cells and desired cells | `ic_previous_and_desired_to_regions` | Changed regions; value owned by caller |
| Regions, desired cells, declared capabilities | `ic_regions_and_capabilities_to_output` | Bounded output bytes; writer owns pending bytes |
| Acknowledged output and its frozen target | `ic_acknowledged_output_to_previous` | Committed previous cells, only after complete emission |
| Terminal events and external byte updates | iLua `named_application_command` | Named commands |
| Named commands and application data | iLua `icurses_event` | Sole selection/data/view state |
| Application state and viewport | iLua `icurses_presentation` | Literal placement and cursor requests |

The functions are named transformations with explicit inputs and outputs.
Caller-owned bounded destination storage avoids allocating/copying a complete
screen for every cell placement. The native adapter never interprets a key as
an application command, stores a selection, or decides table layout.

## Resources

| State or resource | Owner | Borrowing and lifetime |
| --- | --- | --- |
| Terminal/input/output/command descriptors | Application or launcher | POSIX adapter borrows; must remain open until release; never closes them |
| Encountered termios, descriptor flags, signal actions | POSIX adapter | Saved on acquisition and restored on release or partial failure |
| Signal wake pipe and resource generation | POSIX adapter | Created/closed within acquisition/release |
| Decoded-event remainder and escape deadline | C application runner | Explicit values passed to decoder/wait; no hidden read |
| Pending output and frozen target | C application runner | Retained through short/interrupted/would-block writes; next target waits |
| Previous cells | C application runner | Updated only by full acknowledgement |
| Data rows, selection, scroll origin, bindings, view choice, update framing | iLua application | One Lua table, no C mirror |
| UTF-8 decoding, widths, cell placement/comparison, encoding | C mechanisms | Bounded kernels; Lua performs no per-cell hot loop |
| Lua allocation lifetime and accounting | C bridge | Runtime destroyed after terminal cleanup |
| External producer's pipe writer | `icurses-live` data child | Stops on closed pipe; data is ordinary TSV, no presentation authority |

Only `include/icurses_posix.h` admits borrowed native descriptors. The checked
terminal handle is a slot/generation pair. A stale handle fails with
Bad file descriptor (EBADF). One active terminal per process is supported;
a second acquisition fails with Device or resource busy (EBUSY). This is a
declared signal-adapter limit. Lua receives neither a handle nor a descriptor.
There is no native-resource lifetime for a Lua script to guess.

The controlled application bridge opens only base, math, string, table and
UTF-8 libraries. It omits OS, IO, package/native loading and debug access, and
removes direct print/file-loading entry points before loading application
policy. Presentation requests are the application's output path. The separate
standalone interpreter and the explicit pre-terminal `--probe` mode remain
runtime qualification tools. This is an ownership boundary, not an arbitrary
untrusted-script sandbox.

Wait results preserve independent condition bits for data, end, resize,
interruption, deadline expiry and failure. A buffered event forces a zero-time
kernel observation of every source rather than bypassing the command pipe.
Deadlines are absolute monotonic milliseconds; retries reuse the same deadline.
The POSIX adapter owns the native `pollfd` representation. Other backends may
use different native waitables behind their own adapter boundary.

Idriç-facing composition can consume these explicit event/presentation values.
No Idriç compiler or rewrite is a dependency of this implementation.
