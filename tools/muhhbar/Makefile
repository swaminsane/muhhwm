CC      = cc
VERSION = 0.1
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

INCS = -I. -I../.. \
       $(shell pkg-config --cflags x11 xft fontconfig)
LIBS = $(shell pkg-config --libs x11 xft fontconfig)

CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Wno-unused-result -O2 $(INCS) \
          -DVERSION=\"$(VERSION)\"
LDFLAGS = $(LIBS)

SRC = muhhbar.c \
      modules.c \
      modules/battery.c \
      modules/brightness.c \
      modules/volume.c \
      modules/network.c \
      modules/stat.c \
      modules/time.c \
      modules/activity.c \
      modules/streak.c \
      modules/pomodoro.c \
      modules/media.c \
      drw.c \
      util.c

OBJ = $(SRC:.c=.o)

all: muhhbar

muhhbar: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhbar $(OBJ)

install: muhhbar
	mkdir -p $(BINDIR)
	cp -f muhhbar $(BINDIR)
	chmod 755 $(BINDIR)/muhhbar

uninstall:
	rm -f $(BINDIR)/muhhbar

.PHONY: all clean install uninstall
