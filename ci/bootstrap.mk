# Fixed make interface to the shared pinned compiler producer.
.PHONY: identity
identity:
	test -x "$(ICK)"
	"$(ICK)" -fno-link-libatomic -O2 ci/literal-probe.c -o /tmp/icurses-literal-probe
	/tmp/icurses-literal-probe
	sha256sum "$(ICK)" "$$($(ICK) -print-prog-name=cc1)"
	git rev-parse HEAD
