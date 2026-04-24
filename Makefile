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

all: muhhwm muhhtime muhhbar

muhhwm: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

muhhtime: tools/muhhtime.c
	$(CC) -o $@ tools/muhhtime.c -D_XOPEN_SOURCE=700 -D_POSIX_C_SOURCE=200809L

muhhbar:
	$(MAKE) -C tools/muhhbar

%.o: %.c src/muhh.h config.h
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhwm muhhtime $(OBJ)
	$(MAKE) -C tools/muhhbar clean

install: muhhwm muhhtime muhhbar
	mkdir -p $(BINDIR)
	cp -f muhhwm $(BINDIR)
	cp -f muhhtime $(BINDIR)
	cp -f tools/muhhbar/muhhbar $(BINDIR)
	chmod 755 $(BINDIR)/muhhwm
	chmod 755 $(BINDIR)/muhhtime
	chmod 755 $(BINDIR)/muhhbar

uninstall:
	rm -f $(BINDIR)/muhhwm
	rm -f $(BINDIR)/muhhtime
	rm -f $(BINDIR)/muhhbar

.PHONY: all clean install uninstall
