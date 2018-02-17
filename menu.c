#include "kech.h"

#define BUTTON_MAX	4

enum {
	MODE_INVALID,
	MODE_STANDBY,
	MODE_MESSAGE,
	MODE_BUTTONS,
	MODE_AMOUNT,
	MODE_PIN
};

enum {
	MENU_INVALID,
	MENU_STANDBY,
	MENU_PINENTRY,
	MENU_MAIN,
	MENU_WITHDRAW,
	MENU_WITHDRAWN,
	MENU_DEPOSIT,
	MENU_ACCOUNTS,
	MENU_PINCHANGE
};

struct button {
	const char		*text;
	union {
		void		(*handler) (void);
		int		menu;
	} action;
};

struct menu {
	const int	mode;
	const char	*text;
	struct button	buttons_left[BUTTON_MAX],
			buttons_right[BUTTON_MAX];
};

/* static int		menu = MENU_STANDBY; */
static int		menu = MENU_MAIN;
static struct button	*pressed;

static void withdraw(void)
{
	menu = MENU_WITHDRAW;
}

static struct menu menus[] = {
	[MENU_STANDBY] = {
		.mode = MODE_STANDBY
	},
	[MENU_PINENTRY] = {
		.mode = MODE_PIN
	},
	[MENU_MAIN] = {
		.mode = MODE_BUTTONS,
		.buttons_left = {
			{ "Withdraw", { .menu = MENU_WITHDRAW } },
			{ "Deposit", { .menu = MENU_DEPOSIT } }
		},
		.buttons_right = {
			{ "Accounts", { .menu = MENU_ACCOUNTS } },
			{ "Change PIN", { .menu = MENU_PINCHANGE } }
		}
	},
	[MENU_WITHDRAW] = {
		.mode = MODE_BUTTONS,
		.buttons_left = {
			{ "€ 10", { .handler = &withdraw } },
			{ "€ 20", { .handler = &withdraw } },
			{ "€ 50", { .handler = &withdraw } },
			{ "€ 100", { .handler = &withdraw } }
		},
		.buttons_right = {
			{ "€ 200", { .handler = &withdraw } },
			{ "€ 500", { .handler = &withdraw } },
			{ "Custom", { .menu = MENU_WITHDRAWN } },
			{ "Back", { .menu = MENU_MAIN } }
		}
	},
	[MENU_WITHDRAWN] = {
		.mode = MODE_AMOUNT,
		.buttons_right = {
			{ "Back", { .menu = MENU_WITHDRAW } }
		}
	},
	[MENU_DEPOSIT] = {
		.mode = MODE_AMOUNT,
		.buttons_right = {
			{ "Back", { .menu = MENU_MAIN } }
		}
	},
	[MENU_ACCOUNTS] = {
		.mode = MODE_BUTTONS,
		.buttons_right = {
			{ "Back", { .menu = MENU_MAIN } }
		}
	},
	[MENU_PINCHANGE] = {
		.mode = MODE_PIN,
		.buttons_right = {
			{ "Back", { .menu = MENU_MAIN } }
		}
	}
};

static void draw_buttons(struct button *btns, bool side)
{
	int i, n;

	if (!btns)
		return;

	for (n = 0; btns[n].text && n < BUTTON_MAX; n++);

	for (i = 0; i < n; i++)
		draw_button(i, n, side, (&btns[i] == pressed), btns[i].text);
}

static struct button *_button_check(struct button *btns, bool side,
		int x, int y)
{
	int x_btn, y_btn, i, n;

	for (n = 0; btns[n].text && n < BUTTON_MAX; n++);

	/* Calculate button positions and check if coordinates align */
	for (i = 0; i < n; i++) {
		x_btn = side ? w - BUTTON_PADDING - BUTTON_WIDTH :
			BUTTON_PADDING;
		y_btn = BANNER_HEIGHT * h + (h - BANNER_HEIGHT * h) / 2 -
				(n * (BUTTON_HEIGHT + BUTTON_PADDING)) / 2 +
				i * (BUTTON_HEIGHT + BUTTON_PADDING);

		if (x >= x_btn && x <= x_btn + BUTTON_WIDTH &&
				y >= y_btn && y <= y_btn + BUTTON_HEIGHT)
			return &btns[i];
	}

	/*
	 * FIXME Maybe simply perform these calculatations statically you
	 * dickhead?
	 * */

	return 0;
}

bool button_check(bool press, int x, int y)
{
	struct button *btn;

	if (!press && pressed) {
		if (pressed->action.menu)
			menu = pressed->action.menu;
		pressed = NULL;

		return 1;
	}

	if (!(btn = _button_check(menus[menu].buttons_left, 0, x, y)) &&
			!(btn =
			_button_check(menus[menu].buttons_right, 1, x, y)))
		return 0;

	if (press)
		pressed = btn;

	return 1;
}

void draw_menu(void)
{
	draw_background();

	switch (menus[menu].mode) {
	case MODE_MESSAGE:
		/* TODO */
		draw_buttons(menus[menu].buttons_right, 1);
		break;
	case MODE_BUTTONS:
		draw_buttons(menus[menu].buttons_left, 0);
		draw_buttons(menus[menu].buttons_right, 1);
		break;
	case MODE_AMOUNT:
		/* TODO */
		draw_buttons(menus[menu].buttons_right, 1);
		break;
	case MODE_PIN:
		draw_pin();
		draw_buttons(menus[menu].buttons_right, 1);
		break;
	case MODE_STANDBY:
	default:
		break;
	}
}

bool need_input(void)
{
	return menus[menu].mode == MODE_PIN || menus[menu].mode == MODE_AMOUNT;
}
