#ifndef _KECH_H
#define _KECH_H

#include <stdint.h>

enum {
	_NET_WM_NAME,
	_NET_WM_STATE,
	_NET_WM_STATE_FULLSCREEN,
	ATOM_MAX
};

enum {
	MODE_STANDBY,
	MODE_MESSAGE,
	MODE_BUTTONS,
	MODE_AMOUNT,
	MODE_PIN
};

enum {
	MENU_STANDBY,
	MENU_PINENTRY,
	MENU_MAIN,
	MENU_WITHDRAW,
	MENU_WITHDRAWN,
	MENU_DEPOSIT,
	MENU_ACCOUNTS,
	MENU_PINCHANGE
};

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
