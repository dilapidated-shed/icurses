# Recorded workload and regression bounds

The workload creates a real foreground-terminal child, paints a 12×70 table,
observes one second with no input, sends one selection key, sends an external
TSV update through a real pipe, resizes to 17×91, and exits. It repeats with
SIGTERM cleanup and with an edited Lua binding. The live producer launcher is
tested separately. The parent records time until emitted changes are visible
on its PTY master; this is terminal-output visibility, not a physical display
scanout or IME measurement. Millisecond readings may be zero due to clock
resolution.

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

Native-host regression bounds use four times the largest observed key/pipe
latency, rounded up to 32 ms, and four times the largest measured idle CPU,
rounded up to 4000 microseconds per second of idle. Deterministic workload
ceilings round the observed allocation count, peak memory and paint bytes up
to 1024 calls, 65536 bytes and 4096 paint bytes. Frames remain at most four;
idle allocations and idle emitted bytes must remain zero. These are workload
bounds, not universal performance guarantees.

Android emulator performance is a separate target and initially records its
measurements without inheriting native-host timing thresholds. Its measured
regression bounds must be recorded after genuine Android execution. The normal
suite's finite operation/deadlock deadlines remain required on every target.
