# Fixed build-tool interface. All consumer C goes through pinned ICK.
# No conversion of literal source; foreign Lua keeps its upstream C spelling.
.DEFAULT_GOAL := all
ICK ?=
AICI ?= ../ai-ci
LUA_SOURCE ?= ../lua
OUT ?= out/host
LUA_REVISION = 87306483cec50f8c750a22dda1d0742246fad756
ICK_REVISION = fbe86e23d55cfec2000c08e61deea2a407fd7175
ICK_FLAGS ?= -fno-link-libatomic
CPPFLAGS = -Iinclude -I"$(LUA_SOURCE)" -DIC_LUA_REVISION='"$(LUA_REVISION)"'
CFLAGS = -std=c17 -O2 -g -Wall -Wextra -Werror
LDLIBS ?= -lm -ldl -lutil
CORE = src/input.c src/cells.c src/output.c src/posix.c
CORE_OBJECTS = $(addprefix $(OUT)/obj/,$(CORE:.c=.o))
LINKER = $(ICK)
LINK_FLAGS = $(ICK_FLAGS)
AR_TOOL = ar
PLATFORM_FLAGS =
ifeq ($(ANDROID_ABI),)
else
NDK_BIN = $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/bin
NDK_SYSROOT = $(NDK)/toolchains/llvm/prebuilt/linux-x86_64/sysroot
HEADER_TARGET_arm64-v8a = aarch64-linux-android
HEADER_TARGET_x86_64 = x86_64-linux-android
TARGET_FLAGS_arm64-v8a = -ffixed-x18
TARGET_FLAGS_x86_64 = -march=x86-64-v2 -mno-avx -mno-movbe
HEADER_TARGET = $(HEADER_TARGET_$(ANDROID_ABI))
TARGET_FLAGS = $(TARGET_FLAGS_$(ANDROID_ABI))
ICK_BUILTIN_INCLUDE = $(shell "$(ICK)" -print-file-name=include)
PLATFORM_FLAGS = $(TARGET_FLAGS) --sysroot="$(NDK_SYSROOT)" -nostdinc -isystem "$(ICK_BUILTIN_INCLUDE)" -isystem "$(NDK_SYSROOT)/usr/include" -isystem "$(NDK_SYSROOT)/usr/include/$(HEADER_TARGET)" -D__ANDROID__ -D__ANDROID_API__=26 -D__ANDROID_MIN_SDK_VERSION__=26 -DBIONIC_IOCTL_NO_SIGNEDNESS_OVERLOAD -fPIC -gdwarf-4 -gno-variable-location-views
LINKER = $(NDK_BIN)/$(HEADER_TARGET)26-clang
LINK_FLAGS = $(TARGET_FLAGS) -pie -Wl,--no-undefined -Wl,-z,text -Wl,-z,nocopyreloc -Wl,-z,max-page-size=16384
AR_TOOL = $(NDK_BIN)/llvm-ar
LDLIBS = -lm -ldl
PLATFORM_INSPECTION = android-inspect
endif
LUA_NAMES = lapi lcode lctype ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio ltests lauxlib lbaselib ldblib liolib lmathlib loslib ltablib lstrlib lutf8lib loadlib lcorolib linit
LUA_OBJECTS = $(addprefix $(OUT)/lua-obj/,$(addsuffix .o,$(LUA_NAMES)))

.PHONY: all preflight test profiles bundle mutants check-policy-edit
all: preflight $(OUT)/test-core $(OUT)/test-posix $(OUT)/test-demo $(OUT)/icurses-demo $(OUT)/icurses-live $(OUT)/ilua
preflight:
	test -x "$(ICK)"
	test "$$(git -C "$(LUA_SOURCE)" rev-parse HEAD)" = "$(LUA_REVISION)"
	git -C "$(LUA_SOURCE)" diff --quiet HEAD --
	test "$$(git -C "$(AICI)" rev-parse HEAD)" = a66e3a9bb1070a338f8c4a67f9b4b4ac39dfb5ec
	mkdir -p "$(OUT)"
ifeq ($(ANDROID_ABI),)
	"$(ICK)" $(ICK_FLAGS) $(CFLAGS) ci/literal-probe.c -o "$(OUT)/literal-probe"
	"$(OUT)/literal-probe"
else
	grep -F 'Pkg.Revision = 27.3.13750724' "$(NDK)/source.properties"
	"$(ICK)" $(PLATFORM_FLAGS) $(CFLAGS) -S ci/literal-probe.c -o "$(OUT)/literal-probe.s"
	"$(LINKER)" $(TARGET_FLAGS) -c "$(OUT)/literal-probe.s" -o "$(OUT)/literal-probe.o"
	"$(LINKER)" $(LINK_FLAGS) "$(OUT)/literal-probe.o" -o "$(OUT)/literal-probe"
endif
	sha256sum "$(ICK)" "$$($(ICK) -print-prog-name=cc1)" > "$(OUT)/compiler.sha256"
	git rev-parse HEAD > "$(OUT)/source-revision.txt"
	! gcc -std=c17 -c ci/literal-probe.c -o "$(OUT)/stock-compiler.o" > "$(OUT)/stock-compiler.log" 2>&1
	test ! -e "$(OUT)/stock-compiler.o"
	grep -E 'stray|not valid|unexpected|undeclared' "$(OUT)/stock-compiler.log"
$(OUT):
	mkdir -p "$@"
$(OUT)/obj/%.o: %.c include/icurses.h include/icurses_posix.h | preflight
	mkdir -p "$(@D)"
ifeq ($(ANDROID_ABI),)
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) -c "$<" -o "$@"
else
	"$(ICK)" $(PLATFORM_FLAGS) $(CPPFLAGS) $(CFLAGS) -S "$<" -o "$(@:.o=.s)"
	"$(LINKER)" $(TARGET_FLAGS) -c "$(@:.o=.s)" -o "$@"
endif
$(OUT)/test-core: $(addprefix $(OUT)/obj/,src/input.o src/cells.o src/output.o tests/test_core.o tests/observer.o)
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
$(OUT)/test-posix: $(CORE_OBJECTS) $(OUT)/obj/tests/test_posix.o $(OUT)/obj/tests/observer.o
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
$(OUT)/test-demo: $(OUT)/obj/src/posix.o $(OUT)/obj/tests/test_demo.o
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
ifeq ($(ANDROID_ABI),)
$(OUT)/lua/liblua.a: | preflight
	mkdir -p "$(OUT)/lua"
	cp "$(LUA_SOURCE)/"*.[ch] "$(OUT)/lua/"
	cp "$(LUA_SOURCE)/makefile" "$(OUT)/lua/makefile"
	$(MAKE) -C "$(OUT)/lua" -f "$(abspath $(LUA_SOURCE))/makefile" CC="$(ICK) $(ICK_FLAGS)" MYCFLAGS="-std=c99 -DLUA_USE_POSIX" MYLDFLAGS= MYLIBS=-ldl liblua.a lua
$(OUT)/ilua: $(OUT)/lua/liblua.a
	cp "$(OUT)/lua/lua" "$@"
else
$(OUT)/lua-obj/%.o: $(LUA_SOURCE)/%.c | preflight
	mkdir -p "$(@D)"
	"$(ICK)" $(PLATFORM_FLAGS) -I"$(LUA_SOURCE)" -std=c99 -O2 -DLUA_USE_POSIX -S "$<" -o "$(@:.o=.s)"
	"$(LINKER)" $(TARGET_FLAGS) -c "$(@:.o=.s)" -o "$@"
$(OUT)/lua/liblua.a: $(LUA_OBJECTS)
	mkdir -p "$(@D)"
	"$(AR_TOOL)" rcs "$@" $^
$(OUT)/ilua: $(OUT)/lua-obj/lua.o $(OUT)/lua/liblua.a
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
endif
$(OUT)/icurses-demo: $(CORE_OBJECTS) $(OUT)/obj/demo/main.o $(OUT)/lua/liblua.a
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
$(OUT)/icurses-live: $(OUT)/obj/demo/live.o
	"$(LINKER)" $(LINK_FLAGS) $^ $(LDLIBS) -o "$@"
test: all profiles
	"$(OUT)/test-core"
	"$(OUT)/test-posix"
	"$(OUT)/icurses-demo" --identity
	"$(OUT)/ilua" tests/runtime.ilua
	"$(OUT)/ilua" tests/policy.ilua demo/table.ilua
	"$(OUT)/test-demo" "$(abspath $(OUT))/icurses-demo" "$(abspath demo/table.ilua)" j
	"$(OUT)/test-demo" --live "$(abspath $(OUT))/icurses-live" "$(abspath $(OUT))/icurses-demo" "$(abspath demo/table.ilua)"
	$(MAKE) check-policy-edit
profiles: | $(OUT)
	"$(ICK)" $(ICK_FLAGS) $(CFLAGS) "$(AICI)/c-source/assignments.c" -o "$(OUT)/source-profile"
	"$(OUT)/source-profile" check include/icurses.h include/icurses_posix.h $(CORE) demo/main.c demo/live.c ci/literal-probe.c tests/test_core.c tests/observer.c tests/test_posix.c tests/test_demo.c
	"$(ICK)" $(ICK_FLAGS) $(CFLAGS) "$(AICI)/src/aici.c" -o "$(OUT)/aici"
	"$(OUT)/aici" verify "$(AICI)/contracts/build-toolchain-v0.contract.tsv" .
check-policy-edit: $(OUT)/test-demo $(OUT)/icurses-demo
	sha256sum "$(OUT)/icurses-demo" > "$(OUT)/before-policy-edit.sha256"
	sed 's/down ← "j"/down ← "n"/' demo/table.ilua > "$(OUT)/alternate.ilua"
	"$(OUT)/test-demo" "$(abspath $(OUT))/icurses-demo" "$(abspath $(OUT))/alternate.ilua" n
	sha256sum -c "$(OUT)/before-policy-edit.sha256"
bundle: all $(PLATFORM_INSPECTION)
	mkdir -p "$(OUT)/bundle"
	cp "$(OUT)/icurses-demo" "$(OUT)/icurses-live" "$(OUT)/ilua" "$(OUT)/literal-probe" "$(OUT)/test-core" "$(OUT)/test-posix" "$(OUT)/test-demo" "$(OUT)/bundle/"
	cp demo/table.ilua tests/runtime.ilua tests/policy.ilua docs/RUNNING.md "$(OUT)/bundle/"
	cp "$(LUA_SOURCE)/README.md" "$(OUT)/bundle/LUA-README.md"
	cp "$(LUA_SOURCE)/lua.h" "$(OUT)/bundle/LUA-NOTICE.h"
	cp "$(OUT)/compiler.sha256" "$(OUT)/source-revision.txt" "$(OUT)/bundle/"
	cd "$(OUT)/bundle" && sha256sum icurses-demo icurses-live ilua literal-probe test-core test-posix test-demo table.ilua runtime.ilua policy.ilua RUNNING.md LUA-README.md LUA-NOTICE.h compiler.sha256 source-revision.txt > SHA256SUMS
	tar -C "$(OUT)" -czf "$(OUT)/icurses-runtime.tar.gz" bundle
	sha256sum "$(OUT)/icurses-runtime.tar.gz" > "$(OUT)/icurses-runtime.tar.gz.sha256"

include ci/mutants.mk
include ci/runtime-controls.mk

.PHONY: android-inspect
android-inspect: all
	"$(NDK_BIN)/llvm-readelf" -h -l -d -r "$(OUT)/icurses-demo" "$(OUT)/icurses-live" "$(OUT)/ilua" "$(OUT)/literal-probe" "$(OUT)/test-core" "$(OUT)/test-posix" "$(OUT)/test-demo" > "$(OUT)/android-elf.txt"
	! grep -E 'R_[A-Z0-9_]+_COPY' "$(OUT)/android-elf.txt"
	grep -F '/system/bin/linker64' "$(OUT)/android-elf.txt"
	grep -F 'Shared library: [libc.so]' "$(OUT)/android-elf.txt"
	printf 'abi\t%s\napi\t26\nndk\t27.3.13750724\nsource_frontend\tick\nplatform_assemble_link\tndk\nandroid_execution\tNOT_RUN\nphysical_miro_c67\tNOT_RUN\n' "$(ANDROID_ABI)" > "$(OUT)/build-receipt.tsv"
