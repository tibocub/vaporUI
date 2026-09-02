CC := gcc
CFLAGS := -Wall

TARGET ?= POSIX
POSIX := src/platform/posix.c

SRCS := $(shell find src -maxdepth 1 -type f -name "*.c")
INCLUDES := includes/%.h

.PHONY: all clean

hello:
	$(CC) $(CFLAGS) examples/hello.c $(SRCS) $($(TARGET)) -o $@

test:
	echo $(TARGET)
	echo $($(TARGET))

clean:
	@find . -type f \(\
		-name "*.o"\
		-o -name "hello"\
		\)\
		-delete\
		-exec echo cleaned: {} +
