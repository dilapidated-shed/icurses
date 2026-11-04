# icurses

Terminal mechanisms in functorial Icky C, with an application policy in the
user's modified iLua. Imported source studies remain intact below.

The maintained implementation is under `include/` and `src/`. The live table
application under `demo/` handles selection, scrolling, external pipe updates,
Unicode labels and resizing. C owns decoding, placement, comparison and
transactional output; iLua owns data, commands, bindings and presentation.

- [Ownership and transformation chain](docs/OWNERSHIP.md)
- [Terminal and Unicode limits](docs/LIMITS.md)
- [Runtime instructions](docs/RUNNING.md)
- [Measured workload and regression bounds](docs/MEASUREMENTS.md)
- [Starting work and provenance](docs/PROVENANCE.md)
- [Required shared runtime admission blocker](docs/SHARED-ADMISSION-BLOCKER.md)

Build through the pinned shared ICK producer and the fixed Make interface;
`build.grease` exposes that interface for an actual Grease runtime. There is
no stock-C acceptance path. Android compilation uses literal Icky source in
ICK, followed by separately declared NDK assembly/link stages. Physical MIRO
C67 acceptance remains separate from Android emulator execution.

## Imported source

### radare2 `libr_cons`

[`libr_cons/`](libr_cons/) contains the complete, unmodified radare2 console
source directory: 65 tracked console files, plus public headers and upstream
license material (77 upstream files total). The snapshot is pinned to radare2
commit `596397d15faaa4decd7367a79e1b8f1a5cc28451`.

- [Provenance, layout, and build boundary](libr_cons/UPSTREAM.md)
- [Reading notes: code observations and possible relevance](notes/libr_cons.md)
- [Per-file source manifest and checksums](libr_cons/SOURCE_MANIFEST.json)
- [Reproducible importer](%5F/import_libr_cons.py)
- [Original investigation and discussion links](https://github.com/isomorphisms/icurses/issues/1)

The imported console subtree reproduces upstream Git tree
`391ee63d66cc398b51ca9182ee06c65a278dfb3d` exactly.

### PDCurses

[`pdcurses/`](pdcurses/) contains the complete tracked PDCurses source tree at
commit `2b6a9e920dc14523829b0b9495f2e3281f03cf29`: 161 upstream files, including
core source, public headers, documentation, demos, build files, and the DOS,
OS/2, Windows-console, X11, SDL1, and SDL2 backends. The pinned upstream tree is
`00246d2b8f4082a1ca4b385df53d1c1b52405570`.

- [PDCurses provenance](pdcurses/UPSTREAM.md)
- [Upstream paths, modes, blob IDs, and sizes](pdcurses/SOURCE_MANIFEST.txt)

`UPSTREAM.md` and `SOURCE_MANIFEST.txt` are local provenance files; the remaining
contents of `pdcurses/` are the pinned upstream tree. The one-off import workflow
was removed after the source landed.

These imports are source for inspection. They are not standalone-build,
ncurses-compatibility, or platform-acceptance claims, and nothing has been
pruned or translated yet.
