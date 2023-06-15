# tool macros
CC := gcc
CFLAGS := -Wall -Werror -Wall -Wextra $(shell pkg-config --cflags dbus-1)
LFLAGS := -ldbus-1
DBGFLAGS := -g

BIN_PATH := bin
DBG_PATH := debug

build: main.o
	gcc main.o $(LFLAGS) -o sbw

main.o: main.c
	gcc -c $(CFLAGS) main.c

clean:
	rm *.o
	rm sbw

install: build
	mkdir -p ~/.local/bin
	cp sbw ~/.local/bin/
