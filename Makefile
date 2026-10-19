CC := gcc
CFLAGS := -Wall

TARGET ?= POSIX
POSIX := src/platform/posix.c

SRCS := $(shell find src -maxdepth 2 -type f -name "*.c" -not -path "src/platform/*")
EXAMPLES := $(patsubst examples/%.c,build/%,$(wildcard examples/*.c))

.PHONY: all test clean

# every example builds; they double as manual regression checks (see
# the header comment of each one for what to verify)
all: $(EXAMPLES)

build/%: examples/%.c $(SRCS) $($(TARGET)) | build
	$(CC) $(CFLAGS) $< $(SRCS) $($(TARGET)) -o $@

build:
	mkdir -p build

# automated checks: input decoder
test: build
	$(CC) $(CFLAGS) -Isrc tests/input.c src/input.c src/vt.c src/utf8.c -o build/input-test
	build/input-test
	$(CC) $(CFLAGS) tests/widgets.c src/widgets/*.c src/clipboard.c src/node.c src/screen.c src/utf8.c -o build/widgets-test
	build/widgets-test

clean:
	rm -rf build
