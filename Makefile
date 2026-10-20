CC := gcc
CFLAGS := -Wall

# make                                              -> POSIX (Linux/macOS/BSD)
# make TARGET=WIN32 CC=x86_64-w64-mingw32-gcc        -> Windows .exe (picker and event-loop are POSIX-only for now)
TARGET ?= POSIX
POSIX := src/platform/posix.c
WIN32 := src/platform/win32.c
ifeq ($(TARGET),WIN32)
EXE := .exe
SKIP := examples/picker.c examples/event-loop.c
endif

SRCS := $(shell find src -maxdepth 2 -type f -name "*.c" -not -path "src/platform/*")
EXAMPLES := $(patsubst examples/%.c,build/%$(EXE),$(filter-out $(SKIP),$(wildcard examples/*.c)))

.PHONY: all test clean

# every example builds; they double as manual regression checks (see
# the header comment of each one for what to verify)
all: $(EXAMPLES)

build/%$(EXE): examples/%.c $(SRCS) $($(TARGET)) | build
	$(CC) $(CFLAGS) $< $(SRCS) $($(TARGET)) -o $@

build:
	mkdir -p build

# automated checks: input decoder
test: build
	$(CC) $(CFLAGS) -Isrc tests/input.c src/input.c src/vt.c src/utf8.c -o build/input-test
	build/input-test
	$(CC) $(CFLAGS) tests/widgets.c tests/platform_stub.c $(SRCS) -o build/widgets-test
	build/widgets-test

clean:
	rm -rf build
