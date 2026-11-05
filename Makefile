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

# terminal widget (POSIX only): libvterm is built once into objects and linked into the examples that use it
VTERM_SRCS := $(wildcard third_party/libvterm/src/*.c)
VTERM_OBJS := $(patsubst third_party/libvterm/src/%.c,$(BUILD)/vterm/%.o,$(VTERM_SRCS))
TERM_EXAMPLES :=
ifeq ($(TARGET),POSIX)
TERM_SRCS := src/ext/terminal.c $(VTERM_OBJS)
TERM_EXAMPLES := $(BUILD)/windows
$(TERM_EXAMPLES): CFLAGS += -DVUI_TERMINAL
$(TERM_EXAMPLES): $(TERM_SRCS)
endif

.PHONY: all test clean

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

test: build
	$(CC) $(CFLAGS) -Isrc tests/input.c src/input.c src/vt.c src/utf8.c -o build/input-test
	build/input-test
	$(CC) $(CFLAGS) tests/widgets.c tests/platform_stub.c $(SRCS) -o build/widgets-test
	build/widgets-test

clean:
	rm -rf build
