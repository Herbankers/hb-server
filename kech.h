#ifndef _KECH_H
#define _KECH_H

#include <stdint.h>

enum {
	_NET_WM_NAME,
	_NET_WM_STATE,
	_NET_WM_STATE_FULLSCREEN,
	ATOM_MAX
};

#define MODE_STANDBY	0
#define MODE_PINENTRY	1
#define MODE_MAIN	2
#define MODE_WITHDRAW	3
#define MODE_WITHDRAWM	4
#define MODE_DEPOSIT	5
#define MODE_ACCOUNTS	6
#define MODE_PINCHANGE	7

#define MSG_TYPE_INV	0
#define MSG_TYPE_NUM	1
#define MSG_TYPE_CARD	2

struct msg {
	/* Message type */
	const uint8_t	type;
	/* Message length in bytes */
	const int	length;
	/* Message */
	const char	msg[];
};

extern const pthread_mutex_t lock;

void die(const char *errstr, ...);

void *draw();
void draw_fini(void);

#endif
