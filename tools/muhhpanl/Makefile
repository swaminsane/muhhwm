# muhhpanl – standalone slide‑down panel

CC      = cc
VERSION = 0.1
PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

INCS = -I. -Icommon \
       $(shell pkg-config --cflags x11 xft fontconfig)

LIBS = -lXext $(shell pkg-config --libs x11 xft fontconfig) -lm

CFLAGS   = -std=c99 -pedantic -Wall -Wextra -Wno-unused-parameter -O2 $(INCS) \
           -D_POSIX_C_SOURCE=200809L -DVERSION=\"$(VERSION)\"

LDFLAGS  = $(LIBS)

SRC = muhhpanl.c \
      $(wildcard common/*.c) \
      $(wildcard modules/left/*.c) \
      $(wildcard modules/middle/*.c) \
      $(wildcard modules/right/*.c) \
      $(wildcard modules/bottom/*.c) \
      $(wildcard modules/topstrip/*.c) \
      $(wildcard timeline/*.c)

OBJ = $(SRC:.c=.o)

all: muhhpanl

muhhpanl: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f muhhpanl $(OBJ)

install: muhhpanl
	mkdir -p $(BINDIR)
	cp -f muhhpanl $(BINDIR)
	chmod 755 $(BINDIR)/muhhpanl

uninstall:
	rm -f $(BINDIR)/muhhpanl

.PHONY: all clean install uninstall
