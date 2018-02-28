#
# Makefile
#

CC		:= gcc
LD		:= gcc

PKGLIST		= libssl libcrypto mariadb

MAKEFLAGS	:= -s

CFLAGS		:= -Wall -Wextra -unused-parameter -Wpedantic -std=c99 -D_XOPEN_SOURCE=700 `pkg-config --cflags $(PKGLIST)` -g -O0 #-s -Os
LDFLAGS		:= -lpthread -lscrypt `pkg-config --libs $(PKGLIST)`

kech-server-o	= main.o session.o

all: kech-server

clean:
	[ -f kech-server ] && echo -e "  RM      kech-server" || true
	rm -f kech-server
	find . -type f -name '*.o' -delete -exec sh -c "echo '  RM      {}'" \;

%.o: %.c
	echo -e "  CC      $<"
	$(CC) $(CFLAGS) -c $< -o $@

kech-server: $(kech-server-o)
	echo -e "  LD      $@"
	$(LD) -o $@ $(kech-server-o) $(LDFLAGS)
