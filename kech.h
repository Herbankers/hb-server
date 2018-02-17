#ifndef _KECH_H
#define _KECH_H

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define KEY_BUFFER	6
#define AMOUNT_LENGTH	6
#define PIN_LENGTH	4

#define WIDTH		800
#define HEIGHT		600

#define BANNER_HEIGHT	0.2

#define PADDING_TOP	20
#define PADDING_BOTTOM	20

#define BUTTON_WIDTH	150
#define BUTTON_HEIGHT	75
#define BUTTON_PADDING	20

#define BULLET_RADIUS	40
#define BULLET_PADDING	40

#define TEXTBOX_HEIGHT	(BULLET_PADDING * 2 + BULLET_RADIUS)
#define TEXTBOX_WIDTH	(BULLET_PADDING + (BULLET_RADIUS * 2 + BULLET_PADDING) \
			* KEY_BUFFER)
#define PINBOX_WIDTH	(BULLET_PADDING + (BULLET_RADIUS * 2 + BULLET_PADDING) \
			* PIN_LENGTH)

enum {
	_NET_WM_NAME,
	_NET_WM_STATE,
	_NET_WM_STATE_FULLSCREEN,
	ATOM_MAX
};

extern const pthread_mutex_t lock;

extern uint16_t		w, h;

void die(const char *errstr, ...);

bool button_check(bool press, int x, int y);
void draw_menu(void);
bool need_input(int n);

void draw_background(void);
void draw_text(int x, int y, int w, int h, const char *text, int size);
void draw_button(int n, int t, bool side, bool pressed, const char *text);
void draw_pin(void);
void *draw();
void draw_fini(void);

#endif
