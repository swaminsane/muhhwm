# muhhpanel - Makefile

CC      = cc
VERSION = 0.1
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

INCS = -I. -Icommon -I.. \
       $(shell pkg-config --cflags x11 xft fontconfig)

LIBS = $(shell pkg-config --libs x11 xft fontconfig) -lm

CFLAGS   = -std=c99 -pedantic -Wall -Wextra -O2 $(INCS) \
           -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\"
LDFLAGS  = $(LIBS)

SRC = muhhpanel.c \
      $(wildcard common/*.c) \
      $(wildcard modules/left/*.c) \
      $(wildcard modules/middle/*.c) \
      $(wildcard modules/right/*.c) \
      $(wildcard modules/bottom/*.c) \
	  $(wildcard modules/topstrip/*.c) \
      $(wildcard timeline/*.c)

OBJ = $(SRC:.c=.o)

all: muhhpanel muhhtoggle

muhhpanel: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)


muhhtoggle: muhhtoggle.c
	$(CC) -o $@ muhhtoggle.c $(shell pkg-config --cflags --libs x11)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhpanel muhhtoggle $(OBJ)

install: muhhpanel muhhtoggle
	mkdir -p $(BINDIR)
	cp -f muhhpanel $(BINDIR)
	cp -f muhhtoggle $(BINDIR)
	chmod 755 $(BINDIR)/muhhpanel
	chmod 755 $(BINDIR)/muhhtoggle

uninstall:
	rm -f $(BINDIR)/muhhpanel
	rm -f $(BINDIR)/muhhtoggle

.PHONY: all clean install uninstall
