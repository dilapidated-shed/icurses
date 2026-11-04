# Fixed make interface to the shared pinned compiler producer.
.PHONY: identity archive
identity:
	test -x "$(ICK)"
	"$(ICK)" -fno-link-libatomic -O2 ci/literal-probe.c -o /tmp/icurses-literal-probe
	/tmp/icurses-literal-probe
	sha256sum "$(ICK)" "$$($(ICK) -print-prog-name=cc1)"
	git rev-parse HEAD
archive:
	mkdir -p out/compiler
	cp -a "$(dir $(ICK))/../." out/compiler/
	strip --strip-debug out/compiler/bin/gcc out/compiler/libexec/gcc/x86_64-pc-linux-gnu/17.0.0/cc1
	out/compiler/bin/gcc -fno-link-libatomic -O2 ci/literal-probe.c -o out/compiler/probe
	out/compiler/probe
	sha256sum out/compiler/bin/gcc out/compiler/libexec/gcc/x86_64-pc-linux-gnu/17.0.0/cc1 > out/compiler/stripped-identity.sha256
	tar -C out -czf out/qualified-compiler.tar.gz compiler
