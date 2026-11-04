# Deliberately broken production transformations; acceptance must reject them.
.PHONY: mutants
mutants: $(OUT)/test-core $(OUT)/test-posix
	mkdir -p "$(OUT)/mutants/buffered/src" "$(OUT)/mutants/pipe/src" "$(OUT)/mutants/commit/src" "$(OUT)/mutants/restoration/src"
	cp src/posix.c "$(OUT)/mutants/buffered/src/posix.c"
	cp src/posix.c "$(OUT)/mutants/pipe/src/posix.c"
	cp src/output.c "$(OUT)/mutants/commit/src/output.c"
	cp src/posix.c "$(OUT)/mutants/restoration/src/posix.c"
	patch -d "$(OUT)/mutants/buffered" -p1 < tests/mutants/buffered.patch
	patch -d "$(OUT)/mutants/pipe" -p1 < tests/mutants/pipe.patch
	patch -d "$(OUT)/mutants/commit" -p1 < tests/mutants/commit.patch
	patch -d "$(OUT)/mutants/restoration" -p1 < tests/mutants/restoration.patch
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) src/input.c src/cells.c src/output.c "$(OUT)/mutants/buffered/src/posix.c" tests/test_posix.c tests/observer.c $(LDLIBS) -o "$(OUT)/mutants/buffered-test"
	! "$(OUT)/mutants/buffered-test" readiness > "$(OUT)/mutants/buffered.log" 2>&1
	grep -F 'result.conditions & IC_WAIT_TERMINAL' "$(OUT)/mutants/buffered.log"
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) src/input.c src/cells.c src/output.c "$(OUT)/mutants/pipe/src/posix.c" tests/test_posix.c tests/observer.c $(LDLIBS) -o "$(OUT)/mutants/pipe-test"
	! "$(OUT)/mutants/pipe-test" readiness > "$(OUT)/mutants/pipe.log" 2>&1
	grep -F 'result.conditions & IC_WAIT_COMMAND' "$(OUT)/mutants/pipe.log"
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) src/input.c src/cells.c "$(OUT)/mutants/commit/src/output.c" tests/test_core.c tests/observer.c -o "$(OUT)/mutants/commit-test"
	! "$(OUT)/mutants/commit-test" > "$(OUT)/mutants/commit.log" 2>&1
	grep -F '!ic_acknowledged_output_to_previous' "$(OUT)/mutants/commit.log"
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) src/input.c src/cells.c src/output.c "$(OUT)/mutants/restoration/src/posix.c" tests/test_posix.c tests/observer.c $(LDLIBS) -o "$(OUT)/mutants/restoration-test"
	! "$(OUT)/mutants/restoration-test" restoration > "$(OUT)/mutants/restoration.log" 2>&1
	grep -F 'same_modes(&encountered, &restored)' "$(OUT)/mutants/restoration.log"
