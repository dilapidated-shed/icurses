# Runtime bundle

This bundle is a terminal application. It contains the native executable,
the statically embedded modified Lua runtime, a standalone interpreter for the
same runtime, literal application source, and acceptance executables.
The checked `libicurses.a` and public headers are included for consumers on
build hosts; the terminal/device path uses the prebuilt executables.

Verify SHA256SUMS before execution from the extracted bundle directory.
Run `./icurses-demo table.ilua` in an xterm-compatible UTF-8 terminal.
For a continuously updating external producer, run `./icurses-live` with the
absolute paths of `icurses-demo` and `table.ilua` as its two arguments.
Use arrows or j/k to select, v to switch presentation, and q to exit.
The optional second argument is a borrowed descriptor for an ordinary command
pipe. A producer writes lines of `row<TAB>label<TAB>value<NEWLINE>`.
Indices are 1–128; labels and values are literal text. The test-demo executable
creates a real pipe and terminal process and exercises this path automatically.

For a runtime-only acceptance run, execute `./test-core`, `./test-posix`,
`./icurses-demo --identity`, `./ilua runtime.ilua`, then
`./test-demo ABSOLUTE_PATH_TO_icurses-demo ABSOLUTE_PATH_TO_table.ilua j`.
No compilation is required on the target.

The ARM64 Android bundle targets MIRO C67 / Android 14, Bionic, API 26 or later.
Extract it into executable app-private storage such as the terminal application's
own files directory. Shared Android downloads storage may prohibit execution.
Do not install compilers on the phone. Physical-device acceptance is separate
from the build and Android emulator results.

Edit the bindings or presentation in table.ilua and rerun the same executable.
An intentional edit requires recomputing the package manifest; an unexpected
script alteration must fail verification. The acceptance suite separately
records the executable digest before and after its binding edit.
