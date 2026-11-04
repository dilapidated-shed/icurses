# Recorded workload and regression bounds

The workload creates a real foreground-terminal child, paints a 12×70 table,
observes one second with no input, sends one selection key, sends an external
TSV update through a real pipe, resizes to 17×91, and exits. It repeats with
SIGTERM cleanup and with an edited Lua binding. The live producer launcher is
tested separately. The parent records time until emitted changes are visible
on its PTY master; this is terminal-output visibility, not a physical display
scanout or IME measurement. Millisecond readings may be zero due to clock
resolution.

The application accounts for every Lua allocator call and its live/peak bytes.
Owned native frame/output storage makes three allocations once, with their
exact byte total recorded; the C kernels allocate nothing per cell or frame.
This is owned-allocation accounting, not an assertion about every internal
allocation that a platform libc might perform.

At exact source `f43c7043baee91c0915c678c90e193ef546cd475`, host job
126782523852 in run 41889422129 passed all four application executions:

| Metric | Observed host range |
| --- | --- |
| Startup to initial output | 1–2 ms |
| Key to changed output | 0–1 ms |
| Pipe update to changed output | 0 ms |
| Idle CPU, one-second interval | 0 clock ticks at 100 Hz |
| Idle terminal bytes | 0 |
| Paint frames / bytes | 4 / 3540 |
| Lua allocation calls | 841–880 |
| Lua peak live bytes | 56013–57801 |

The retrieved archive (artifact 13352614155) was digest-verified and executed
without rebuilding from an unrelated directory on a separate Ubuntu workspace.
Its two runs measured 5–8 ms startup, 0–2 ms key latency, 1 ms pipe latency,
zero idle CPU ticks, four frames and 3540 paint bytes. This is a second host
execution, not Android execution.

The subsequent development run measures idle CPU inside the actual application
with `CLOCK_PROCESS_CPUTIME_ID`, avoiding a dependency on a parent's access to
`/proc/<child>/stat`. Four runs recorded 49–857 microseconds CPU over
1013–1020 ms idle, zero allocations in that wait, 0–7 ms key latency, 0–6 ms
pipe latency, and the same four frames/3540 paint bytes. These development
measurements include the subsequent instrumentation and will be superseded by
the final exact-head CI logs; they are not attributed to the earlier commit.

The `github-ubuntu-24.04-x86_64` regression profile uses four times the largest observed key/pipe
latency, rounded up to 32 ms, and four times the largest measured idle CPU,
rounded up to 4000 microseconds per second of idle. Deterministic workload
ceilings round the observed allocation count, peak memory and paint bytes up
to 1024 calls, 65536 bytes and 4096 paint bytes. Frames remain at most four;
idle allocations and idle emitted bytes must remain zero. These are workload
bounds, not universal performance guarantees.

One later disposable-workspace invocation measured 83 ms key and 30 ms pipe
latency while package construction ran concurrently. The 32 ms check failed;
that failure is retained and the limit was not increased. A subsequent isolated
run of the same executable measured 0–3 ms key and 0–1 ms pipe latency across
normal, SIGTERM and actual terminal Ctrl-C exits. CI now finishes construction
and substitution controls before running the measured workload, so the
regression workload does not include concurrent compiler/packager work. The
failure alone does not establish a lower-level cause for the delay.
Further scratch-host runs also showed timing variation. Timing bounds are now
selected by explicit execution-target profile: the hosted CI job requires the
unchanged 32 ms / 4000 µs limits, while an unprofiled machine records timing and
labels its timing regression check NOT_RUN. Shared architecture and OS alone
do not qualify another execution target. Deterministic frame/byte/allocation
limits and functional deadlines remain enforced everywhere.

Android emulator performance is a separate target. Exact source
`a05d1f8ddcc2e7f9fb1c3aba6e126a421cf67621`, run 41897197358, passed the
complete API-34 x86_64 Bionic suite. Artifact 13355897610 was independently
retrieved and ZIP-digest checked. The recorded image fingerprint was
`Android/sdk_phone64_x86_64/emu64x:14/UE1A.230829.036.A1/11228894:userdebug/test-keys`.

| Emulator metric | Three observed executions |
| --- | --- |
| Startup | 6–13 ms |
| Key update | 0 ms |
| Pipe update | 0–1 ms |
| Idle process CPU | 29–56 µs over 1013–1033 ms |
| Idle allocations / emitted bytes | 0 / 0 |
| Paint frames / bytes | 4 / 3540 |
| Lua calls / peak bytes | 740–779 / 46126–50170 |
| Native allocations / bytes | 3 / 1688624 |

The resulting `android-api34-x86_64-emu64x` regression profile permits 4 ms
key/pipe latency (four times the measured maximum) and 256 µs idle CPU per
second (four times the measured maximum, rounded up to a power of two).
These are now explicit emulator-only gates. The normal suite's finite
operation/deadlock deadlines and deterministic resource bounds remain required
on every target. ARM64 physical timing remains NOT_RUN.
