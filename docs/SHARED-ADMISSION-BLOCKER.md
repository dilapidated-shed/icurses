# Shared runtime admission remains blocked

This implementation's actual ICK/iLua host execution is separate from the
existing shared runtime/executable admission contract. It does not issue a
replacement admission decision.

Owner: [Flexible Pipes issue #51](https://github.com/isomorphisms/flexible-pipes/issues/51).
The current [isomorphisms/flexible-pipes PR #52, “Qualify modified Icky Lua
interpreter and embedded runtime through exact ICK”](https://github.com/isomorphisms/flexible-pipes/pull/52)
is at `68c53d0daa64ec998701060abaaf225752ea1e93`. Its unchanged prerequisite
record, `ci/modified-lua-prerequisite.tsv`, declares:

```text
status BLOCKED
blocker compatible_cpp_free_grease_not_independently_accepted
required_target x86_64-linux-gnu
```

Its `modified-lua-runtime.yml` prerequisite job stops with exit 1 before invoking
the shared qualifier. The requested reusable identity contract therefore has
no current accepted runtime/package to consume here. `build.grease` is retained
as a Grease-facing interface and is NOT_RUN; no other shell was used to claim
its parsing or execution.

The owning runtime work is [Grease issue #58](https://github.com/dilapidated-shed/grease/issues/58),
with acquisition/closure policy in [Cat Food issue #131](https://github.com/isomorphisms/catfood/issues/131).
The later current-source correction in Grease #58 establishes that the old
parse-only option defect was repaired. It still records unfinished inherited
semantics and independent static/transitive C++ exclusion and package admission.
Do not restate the historical missing-set or missing-parse-option errors as
current. A historical C++ executable or the partial D candidate cannot be
renamed or installed to bypass this owner prerequisite.

The shared qualifier also pins a historical exact ICK executable closure for
its runtime build. Accepting a newly built compiler tuple requires the owning
contract's qualification, not changing that pin locally to whichever compiler
the consumer happens to have. iCurses uses the current pinned shared ICK
producer for its independent functional work and records that exact tuple.

Affected acceptance: shared language/runtime executable-identity admission;
Grease build-entry parsing/execution; whole-job completion. Independent C
mechanism, application, host, Android build/emulator and runtime bundle work
continues. Green consumer tests cannot close this external obligation.

Finish condition: the existing owners supply an accepted compatible C++-free
Grease package/closure and an approved compiler/runtime tuple; invoke the
unchanged shared positive/negative qualifier, link its qualified runtime into
the actual consumer, and rerun exact-head host/Android acceptance. Keep the
original owner issue/PR as the obligation rather than creating a parallel gate.
