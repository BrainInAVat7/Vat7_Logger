# Makefile for posix Vat7 Logger

.PHONY: all clean posix sdl run-demo run-demo-sdl

# Log level setting
# 0 off, 1 critical, 2 error, 3 warning, 4 info, 5 debug, 6 trace
LOG_LEVEL ?= 6

# File info
POSIX_SRC = \
	src/vt7_log.c \
	src/vt7_log_demo.c \
	../Vat7_PrecisionTiming/src/vt7_pt_posix.c

SDL_SRC = \
	src/vt7_log.c \
	src/vt7_log_demo.c \
	../Vat7_PrecisionTiming/src/vt7_pt_sdl.c

POSIX_DEMO = bin/vt7_log_demo
SDL_DEMO = bin/vt7_log_demo_sdl

# Build flags and extensions
CFLAGS = -std=c17 \
	-DVT7_LOG_LEVEL=$(LOG_LEVEL) \
	-I./include \
	-I./../Vat7_PrecisionTiming/include \
	-Werror \
	-Wall -Wextra -Wpedantic \
	-Wshadow \
	-Wconversion -Wsign-conversion \
	-Wcast-align \
	-Wstrict-prototypes \
	-Wmissing-prototypes \
	-Wformat=2 \
	-Wundef \
	-Wnull-dereference \
	-Wdouble-promotion \
	-Wimplicit-fallthrough=5

all: posix


$(POSIX_DEMO): $(POSIX_SRC)
	mkdir -p bin
	mkdir -p log
	$(CC) $(CFLAGS) -o $(POSIX_DEMO) $(POSIX_SRC)

$(SDL_DEMO): $(SDL_SRC)
	mkdir -p bin
	mkdir -p log
	$(CC) $(CFLAGS) -o $(SDL_DEMO) $(SDL_SRC) -lSDL3

posix: $(POSIX_DEMO)


sdl: $(SDL_DEMO)


run-demo: $(POSIX_DEMO)
	./$(POSIX_DEMO)
	cat log/logging.log

run-demo-sdl: $(SDL_DEMO)
	./$(SDL_DEMO)
	cat log/logging.log

clean:
	rm -rf bin
	rm -rf log
