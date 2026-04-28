# muhhwm - Makefile

CC      = cc
VERSION = 0.1

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin
MANDIR  = $(PREFIX)/share/man/man1

INCS = -I. -Isrc \
       $(shell pkg-config --cflags x11 xft xinerama fuse3)

LIBS = $(shell pkg-config --libs x11 xft xinerama fuse3 fontconfig) \
       -lpthread

CFLAGS   = -std=c99 -pedantic -Wall -Wextra -O2 $(INCS) \
           -DVERSION=\"$(VERSION)\"
LDFLAGS  = $(LIBS)

SRC = src/muhh.c \
      src/state.c \
      src/x11.c \
      src/bar.c \
      src/rules.c \
      src/serialize.c \
      src/fs.c \
	  src/activity.c \
	  src/strip.c \
      src/drw.c \
	  src/topbar.c \
      src/util.c

OBJ = $(SRC:.c=.o)

config.h:
	cp config.def.h config.h

all: muhhwm muhhtime

muhhwm: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

muhhtime: tools/muhhtime.c
	$(CC) -o $@ tools/muhhtime.c -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L

%.o: %.c src/muhh.h config.h
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhwm muhhtime $(OBJ)

install: muhhwm muhhtime
	mkdir -p $(BINDIR)
	cp -f muhhwm $(BINDIR)
	cp -f muhhtime $(BINDIR)
	chmod 755 $(BINDIR)/muhhwm
	chmod 755 $(BINDIR)/muhhtime

uninstall:
	rm -f $(BINDIR)/muhhwm
	rm -f $(BINDIR)/muhhtime

.PHONY: all clean install uninstall
