#
# Makefile
#

CC		:= gcc
LD		:= gcc

PKGLIST		= cairo pangocairo xcb xcb-keysyms xcb-util

MAKEFLAGS	:= -s

CFLAGS		:= -Wall -Wextra -unused-parameter -Wpedantic -std=c99 -D_XOPEN_SOURCE=700 `pkg-config --cflags $(PKGLIST)` -g -O0 #-s -Os
LDFLAGS		:= -lpthread `pkg-config --libs $(PKGLIST)`

kech-o	= main.o draw.o menu.o

all: kech

clean:
	[ -f kech ] && echo -e "  RM      kech" || true
	rm -f kech
	find . -type f -name '*.o' -delete -exec sh -c "echo '  RM      {}'" \;

%.o: %.c
	echo -e "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

kech: $(kech-o)
	echo -e "  LD      $@"
	$(LD) -o $@ $(kech-o) $(LDFLAGS)
