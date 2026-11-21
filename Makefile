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
WASM := src/platform/wasm.c
# make TARGET=WASM                                   -> browser build (needs emsdk, see docs/wasm.md): build/wasm/*.js + .wasm
ifeq ($(TARGET),WASM)
CC := emcc
EXE := .js
BUILD := build/wasm
SKIP := examples/event-loop.c
CFLAGS += -O2 -sASYNCIFY -sMODULARIZE -sEXPORT_NAME=createVui -sEXPORTED_RUNTIME_METHODS=FS -sFORCE_FILESYSTEM -sEXIT_RUNTIME=1
endif
BUILD ?= build
ifeq ($(TARGET),WIN32)
EXE := .exe
SKIP := examples/event-loop.c
endif

SRCS := $(wildcard src/*.c src/widgets/*.c)
EXAMPLES := $(patsubst examples/%.c,$(BUILD)/%$(EXE),$(filter-out $(SKIP),$(wildcard examples/*.c)))

# terminal widget (POSIX, and Windows through ConPTY): libvterm is built once into objects and linked into the examples that use it
VTERM_SRCS := $(wildcard third_party/libvterm/src/*.c)
VTERM_OBJS := $(patsubst third_party/libvterm/src/%.c,$(BUILD)/vterm/%.o,$(VTERM_SRCS))
TERM_EXAMPLES :=
ifneq ($(filter POSIX WIN32,$(TARGET)),)
TERM_SRCS := src/ext/terminal.c src/ext/termsel.c $(VTERM_OBJS)
TERM_EXAMPLES := $(BUILD)/windows$(EXE)
$(TERM_EXAMPLES): CFLAGS += -DVUI_TERMINAL
$(TERM_EXAMPLES): $(TERM_SRCS)
endif

.DEFAULT_GOAL := all
.PHONY: all test clean winterm

# every example builds; they double as manual regression checks (see
# the header comment of each one for what to verify)
all: $(EXAMPLES)

$(BUILD)/%$(EXE): examples/%.c $(SRCS) $($(TARGET)) | $(BUILD)
	$(CC) $(CFLAGS) $< $(SRCS) $($(TARGET)) $(if $(filter $@,$(TERM_EXAMPLES)),$(TERM_SRCS)) -o $@

$(BUILD)/vterm/%.o: third_party/libvterm/src/%.c | $(BUILD)/vterm
	$(CC) -O1 -w -Ithird_party/libvterm/include -c $< -o $@

$(BUILD)/vterm:
	mkdir -p $@

$(BUILD):
	mkdir -p $(BUILD)

# automated checks: input decoder
# Windows backend check (needs wine or a Windows console): make wintest && wine build/win-watch.exe
wintest: build
	x86_64-w64-mingw32-gcc $(CFLAGS) -Isrc tests/win_watch.c $(SRCS) $(WIN32) -o build/win-watch.exe

# Windows terminal widget (ConPTY) check: make winterm, run build/win-term.exe (Wine can't run ConPTY output, see the test)
WINVTERM_OBJS := $(patsubst third_party/libvterm/src/%.c,build/winvterm/%.o,$(VTERM_SRCS))

build/winvterm/%.o: third_party/libvterm/src/%.c
	mkdir -p build/winvterm
	x86_64-w64-mingw32-gcc -O1 -w -Ithird_party/libvterm/include -c $< -o $@

winterm: build $(WINVTERM_OBJS)
	x86_64-w64-mingw32-gcc $(CFLAGS) -Isrc tests/win_term.c tests/platform_stub.c $(SRCS) src/ext/terminal.c src/ext/termsel.c $(WINVTERM_OBJS) -o build/win-term.exe

test: build $(VTERM_OBJS)
	$(CC) $(CFLAGS) -Isrc tests/input.c src/input.c src/vt.c src/utf8.c src/scancode.c -o build/input-test
	build/input-test
	$(CC) $(CFLAGS) -Isrc tests/keys.c src/keys.c -o build/keys-test
	build/keys-test
	$(CC) $(CFLAGS) -Isrc tests/winkeys.c src/winkeys.c src/scancode.c src/utf8.c -o build/winkeys-test
	build/winkeys-test
	$(CC) $(CFLAGS) -Isrc tests/vtraw.c src/vtraw.c src/scancode.c -o build/vtraw-test
	build/vtraw-test
	$(CC) $(CFLAGS) -Isrc tests/gpm.c src/gpm.c -o build/gpm-test
	build/gpm-test
	$(CC) $(CFLAGS) -Isrc tests/termsel.c src/ext/termsel.c -o build/termsel-test
	build/termsel-test
	$(CC) $(CFLAGS) tests/widgets.c tests/platform_stub.c $(SRCS) -o build/widgets-test
	build/widgets-test
ifeq ($(TARGET),POSIX)
	$(CC) $(CFLAGS) tests/terminal.c tests/platform_stub.c $(SRCS) $(TERM_SRCS) -o build/terminal-test
	build/terminal-test
endif

clean:
	rm -rf build
