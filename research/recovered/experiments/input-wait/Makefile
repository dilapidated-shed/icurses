# Self-contained POSIX/PTTY experiment. No imported upstream files are compiled.
CC ?= cc
CFLAGS ?= -O2 -g
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
STRICT = -std=c11 -Wall -Wextra -Wpedantic -Werror
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
LDLIBS += -lutil
endif

.PHONY: all test clean
all: test_wait2

test_wait2: wait2.c wait2.h test_wait2.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) wait2.c test_wait2.c -o $@ $(LDFLAGS) $(LDLIBS)

test: test_wait2
	./test_wait2

clean:
	rm -f test_wait2
