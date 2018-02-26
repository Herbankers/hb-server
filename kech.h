#ifndef _KECH_H
#define _KECH_H

#include <stdbool.h>

struct connection {
	int			sock;
	struct sockaddr_in	addr;
};

extern bool verbose;
extern SSL_CTX *ctx;

void *session(void *_conn);

#endif
