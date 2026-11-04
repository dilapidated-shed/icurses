# Exact negative runtime and package controls, separate from production fallback.
STOCK_LUA_SOURCE ?= .stock-lua-source
STOCK_LUA_REVISION = 0b29f408433e92953cc72b1d3e06c7ac8139e439
.PHONY: runtime-controls
runtime-controls: all bundle
	test "$$(git -C "$(STOCK_LUA_SOURCE)" rev-parse HEAD)" = "$(STOCK_LUA_REVISION)"
	mkdir -p "$(OUT)/stock-lua"
	cp "$(STOCK_LUA_SOURCE)/"*.[ch] "$(OUT)/stock-lua/"
	$(MAKE) -C "$(OUT)/stock-lua" -f "$(abspath $(STOCK_LUA_SOURCE))/makefile" CC="$(ICK) $(ICK_FLAGS)" MYCFLAGS="-std=c99 -DLUA_USE_POSIX" MYLDFLAGS= MYLIBS=-ldl liblua.a lua
	! "$(OUT)/stock-lua/lua" tests/runtime.ilua > "$(OUT)/stock-lua.log" 2>&1
	grep -E 'unexpected symbol|syntax error' "$(OUT)/stock-lua.log"
	"$(ICK)" $(ICK_FLAGS) $(CPPFLAGS) $(CFLAGS) $(CORE) demo/main.c "$(OUT)/stock-lua/liblua.a" $(LDLIBS) -o "$(OUT)/wrong-linked-runtime"
	! "$(OUT)/wrong-linked-runtime" --identity > "$(OUT)/wrong-linked-runtime.log" 2>&1
	grep -F 'iLua linked-library probe' "$(OUT)/wrong-linked-runtime.log"
	mkdir -p "$(OUT)/altered-package"
	cp "$(OUT)/bundle/"* "$(OUT)/altered-package/"
	printf '\n-- unexpected altered script bytes\n' >> "$(OUT)/altered-package/table.ilua"
	cd "$(OUT)/altered-package" && ! sha256sum -c SHA256SUMS > ../altered-package.log 2>&1
	grep -F 'table.ilua: FAILED' "$(OUT)/altered-package.log"
