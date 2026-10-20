CC := gcc
CFLAGS := -Wall

# make                                              -> POSIX (Linux/macOS/BSD); on Windows (MSYS2, w64devkit) -> Windows .exe
# make TARGET=WIN32 CC=x86_64-w64-mingw32-gcc        -> cross-build Windows .exe from Linux
ifeq ($(OS),Windows_NT)
TARGET ?= WIN32
endif
TARGET ?= POSIX
POSIX := src/platform/posix.c
WIN32 := src/platform/win32.c
ifeq ($(TARGET),WIN32)
EXE := .exe
SKIP := examples/event-loop.c
endif

SRCS := $(wildcard src/*.c src/widgets/*.c)
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
# Windows backend check (needs wine or a Windows console): make wintest && wine build/win-watch.exe
wintest: build
	x86_64-w64-mingw32-gcc $(CFLAGS) -Isrc tests/win_watch.c $(SRCS) $(WIN32) -o build/win-watch.exe

test: build
	$(CC) $(CFLAGS) -Isrc tests/input.c src/input.c src/vt.c src/utf8.c -o build/input-test
	build/input-test
	$(CC) $(CFLAGS) tests/widgets.c tests/platform_stub.c $(SRCS) -o build/widgets-test
	build/widgets-test

clean:
	rm -rf build
