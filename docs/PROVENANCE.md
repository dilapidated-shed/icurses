# Starting work and implementation provenance

Base: `dilapidated-shed/icurses` main
`663ca05acbab55cf82406d21acf792f4a9b98c6b`. At the implementation refresh the
only other branches were reference studies and no implementation PR was open.

The recovered `icurses-input-wait.patch` has SHA-256
`e089c8a5c4b8f42b2dd2989e086530b6a620e9b0c2e44a9f416b3088712e9847`.
Its historical source/build/readme are retained under `research/recovered/`
without silently changing the original evidence. Those sixteen cases passed
generic compilers in the historical receipt; they are not ICK qualification.
The maintained tests adapt the buffered/simultaneous/delayed/interruption/
deadline/EOF/invalid/restoration cases to the semantic interface. They add
actual terminal output backpressure and a separate foreground-session resize
test. The old synthetic raise(SIGWINCH) remains explicitly synthetic.

The pruned `dilapidated-shed/ncurses` source-first-layout head inspected was
`da8525216cb37d383cfd4905611a3ddbcb46b773`. Its PTY, Bionic API/link split,
runtime bundle, ELF inspection, and emulator receipt machinery informed the
test/build boundary. iCurses does not link ncurses and inherits none of its
runtime passes. Bionic openpty is linked from libc; host Linux uses libutil.
Android NDK r27d (`27.3.13750724`) is retained as the platform stage.

The imported libr_cons and PDCurses trees are unchanged reference material.
Their existing manifests, notices and provenance remain authoritative. No
implementation was copied from those imported trees or ncurses into the new
core. The new independent observer does not call the production renderer.

The modified Lua fork is `isomorphisms/lua` at
`87306483cec50f8c750a22dda1d0742246fad756`; its actual linked runtime executes
the literal symbolic probe. Foreign Lua implementation C retains upstream
spelling. Its copyright/permission notice in lua.h is included in each bundle.
The bridge and application source are separate owned Icky C/iLua profiles.

The compiler producer and source-profile checker are reused from ai-ci at
`a66e3a9bb1070a338f8c4a67f9b4b4ac39dfb5ec`. The producer selects ICK
`fbe86e23d55cfec2000c08e61deea2a407fd7175`, tests literal ←/×/÷ semantics,
and records compiler/runtime bytes. Consumer compilation never substitutes
stock C. Android C-to-assembly and NDK assembly/link are distinct stages.
Bootstrap GCC builds ICK itself; that foreign boundary does not qualify this
consumer. The stock compiler invocation is deliberately a rejection control.

`build.grease` exposes the fixed Make interface through the current Grease
language. It must be run by an actual Grease runtime; no Bash execution is
claimed for it. Make recipes use the same fixed tool interface as the shared
ICK producers and are not a new shell dialect or build framework.
