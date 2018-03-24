#
# Makefile
#

CC		:= gcc

PKGLIST		= libssl mariadb

MAKEFLAGS	:= -s

CFLAGS		:= -Wall -Wextra -Wpedantic -std=c99 -D_XOPEN_SOURCE=700 `pkg-config --cflags $(PKGLIST)` -g -O0 #-s -Os
LDFLAGS		:= -lpthread -lscrypt `pkg-config --libs $(PKGLIST)`

kech-server-o	= \
		src/accounts.o \
		src/iban.o \
		src/login.o \
		src/main.o \
		src/pin_update.o \
		src/session.o \
		src/transactions.o \
		src/transfer.o

all: kech-server

clean:
	[ -f kech-server ] && echo -e "  RM      kech-server" || true
	rm -f kech-server
	find . -type f -name '*.o' -delete -exec sh -c "echo '  RM      {}'" \;

%.o: %.c
	echo -e "  CC      $@"
	$(CC) $(CFLAGS) -c $< -o $@

kech-server: $(kech-server-o)
	echo -e "  CC      $@"
	$(CC) -o $@ $(kech-server-o) $(LDFLAGS)
