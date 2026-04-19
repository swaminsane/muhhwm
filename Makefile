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
      src/drw.c \
      src/util.c

OBJ = $(SRC:.c=.o)

all: muhhwm

muhhwm: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c src/muhh.h config.h
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhwm $(OBJ)

install: muhhwm
	mkdir -p $(BINDIR)
	cp -f muhhwm $(BINDIR)
	chmod 755 $(BINDIR)/muhhwm

uninstall:
	rm -f $(BINDIR)/muhhwm

.PHONY: all clean install uninstall
