#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>

#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "kech.h"

#define TEXT_BUFFER	4

#define WIDTH		800
#define HEIGHT		600

#define BANNER_HEIGHT	0.2

#define PADDING_TOP	20
#define PADDING_BOTTOM	20

#define BUTTON_WIDTH	150
#define BUTTON_HEIGHT	75
#define BUTTON_PADDING	20
#define BUTTON_MAX	4

#define BULLET_RADIUS	40
#define BULLET_PADDING	40

#define TEXTBOX_HEIGHT	(BULLET_PADDING * 2 + BULLET_RADIUS)
#define TEXTBOX_WIDTH	(BULLET_PADDING + (BULLET_RADIUS * 2 + BULLET_PADDING) * TEXT_BUFFER)

struct button {
	const char	*text;
	void		*handler;
};

struct menu {
	int			mode;
	const char		*text;
	const struct button	buttons_left[BUTTON_MAX],
				buttons_right[BUTTON_MAX];
};

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
			{ "Withdraw", NULL },
			{ "Deposit", NULL }
		},
		.buttons_right = {
			{ "Accounts", NULL },
			{ "Change PIN", NULL }
		}
	},
	[MENU_WITHDRAW] = {
		.mode = MODE_BUTTONS,
		.buttons_left = {
			{ "€ 10", NULL },
			{ "€ 20", NULL },
			{ "€ 50", NULL },
			{ "€ 100", NULL }
		},
		.buttons_right = {
			{ "€ 200", NULL },
			{ "€ 500", NULL },
			{ "Custom", NULL },
			{ "Back", NULL }
		}
	},
	[MENU_WITHDRAWN] = {
		.mode = MODE_AMOUNT
	},
	[MENU_DEPOSIT] = {
		.mode = MODE_AMOUNT
	},
	[MENU_ACCOUNTS] = {
		.mode = MODE_BUTTONS,
		.buttons_right = {
			{ "Back", NULL }
		}
	},
	[MENU_PINCHANGE] = {
		.mode = MODE_PIN
	}
};

static xcb_connection_t	*conn;
static xcb_screen_t	*screen;
static xcb_key_symbols_t *keysyms;
static xcb_window_t	win;
static xcb_atom_t	atoms[ATOM_MAX];
static uint16_t		w, h;

static cairo_t		*cr;
static cairo_surface_t	*sur;

static const char		font_desc[] = "sans-serif";
static PangoFontDescription	*font;

/* static int		menu = MENU_STANDBY; */
static int		menu = MENU_MAIN;
static char		buf[TEXT_BUFFER + 1];
static unsigned char	bufi;

static int textw(cairo_t *cr, const char *text)
{
	PangoLayout *layout;
	int w;

	layout = pango_cairo_create_layout(cr);

	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, text, strlen(text));

	pango_cairo_update_layout(cr, layout);

	pango_layout_get_pixel_size(layout, &w, NULL);

	g_object_unref(G_OBJECT(layout));

	return w;
}

static int texth(cairo_t *cr, const char *text)
{
	PangoLayout *layout;
	int h;

	layout = pango_cairo_create_layout(cr);

	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, text, strlen(text));

	pango_cairo_update_layout(cr, layout);

	pango_layout_get_pixel_size(layout, NULL, &h);

	g_object_unref(G_OBJECT(layout));

	return h;
}

void text_draw(int x, int y, int w, int h, const char *text, int size)
{
	PangoLayout *layout;

	pango_font_description_set_size(font, size * PANGO_SCALE);
	layout = pango_cairo_create_layout(cr);

	cairo_move_to(cr, x + (w / 2) - (textw(cr, text) / 2),
			y + (h / 2) - (texth(cr, text) / 2));

	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

	pango_layout_set_font_description(layout, font);
	pango_layout_set_text(layout, text, -1);

	pango_cairo_update_layout(cr, layout);
	pango_cairo_show_layout(cr, layout);

	g_object_unref(G_OBJECT(layout));
}

static void draw_background(void)
{
	cairo_surface_t *logo;

	/* cairo_set_source_rgb(cr, 0.0, 0.0, 0.0); */

	/* cairo_rectangle(cr, 0, 0, w, BANNER_HEIGHT * h);
	cairo_fill(cr); */

	/* cairo_rectangle(cr, 0, BANNER_HEIGHT * h, w, h - BANNER_HEIGHT * h); */

	cairo_set_source_rgb(cr, 0.96, 0.96, 0.96);
	cairo_rectangle(cr, 0, 0, w, h);
	cairo_fill(cr);

	logo = cairo_image_surface_create_from_png("logo.png");
	cairo_set_source_surface(cr, logo, w - 1536, 0);
	cairo_paint(cr);

	cairo_surface_destroy(logo);
}

static void draw_button(int n, int t, bool side, bool pressed, const char *text)
{
	int x, y;

	x = side ? w - BUTTON_PADDING - BUTTON_WIDTH : BUTTON_PADDING;
	y = BANNER_HEIGHT * h + (h - BANNER_HEIGHT * h) / 2 -
			(t * (BUTTON_HEIGHT + BUTTON_PADDING)) / 2 +
			n * (BUTTON_HEIGHT + BUTTON_PADDING);

	if (pressed)
		cairo_set_source_rgb(cr, 0.56, 0.64, 0.68);
	else
		cairo_set_source_rgb(cr, 0.69, 0.75, 0.77);

	cairo_rectangle(cr, x, y, BUTTON_WIDTH, BUTTON_HEIGHT);
	cairo_fill(cr);

	text_draw(x, y, BUTTON_WIDTH, BUTTON_HEIGHT, text, 16);
}

static void draw_buttons(const struct button *btns, bool side)
{
	int i, n;

	if (!btns)
		return;

	for (n = 0; btns[n].text && n < BUTTON_MAX; n++);

	for (i = 0; i < n; i++)
		draw_button(i, n, side, 0, btns[i].text);
}

static void draw_pin(void)
{
	int x, y, i;

	cairo_set_source_rgb(cr, 0.69, 0.75, 0.77);

	x = w / 2 - TEXTBOX_WIDTH / 2;
	y = BANNER_HEIGHT * h + (h - BANNER_HEIGHT * h) / 2 -
			TEXTBOX_HEIGHT / 2;

	cairo_rectangle(cr, x, y, TEXTBOX_WIDTH, TEXTBOX_HEIGHT);
	cairo_fill(cr);

	for (i = 0; i < bufi; i++) {
		cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
		cairo_arc(cr, x + BULLET_RADIUS + BULLET_PADDING +
				(BULLET_RADIUS * 2 + BULLET_PADDING) * i,
				y + TEXTBOX_HEIGHT / 2, BULLET_RADIUS,
				0, 2 * M_PI);
		cairo_fill(cr);
	}
}

static void draw_menu(void)
{
	draw_background();

	switch (menus[menu].mode) {
	case MODE_MESSAGE:
		/* TODO */
		break;
	case MODE_BUTTONS:
		draw_buttons(menus[menu].buttons_left, 0);
		draw_buttons(menus[menu].buttons_right, 1);
		break;
	case MODE_AMOUNT:
		/* TODO */
		break;
	case MODE_PIN:
		draw_pin();
		break;
	case MODE_STANDBY:
	default:
		break;
	}

	xcb_flush(conn);
}

static void geom_update(void)
{
	xcb_depth_iterator_t di;
	xcb_visualtype_iterator_t vi;
	xcb_visualtype_t *visual = NULL;
	xcb_get_geometry_reply_t *g;

	/* Get geometry */
	g = xcb_get_geometry_reply(conn, xcb_get_geometry(conn, win), NULL);

	w = g->width;
	h = g->height;

	free(g);

	/* Get visual */
	for (di = xcb_screen_allowed_depths_iterator(screen); di.rem;
			xcb_depth_next(&di)) {
		for (vi = xcb_depth_visuals_iterator(di.data); vi.rem;
				xcb_visualtype_next(&vi)) {
			if (screen->root_visual == vi.data->visual_id) {
				visual = vi.data;
				break;
			}
		}
	}

	if (!visual)
		die("unable to retrieve visual type\n");

	/* Create Cairo drawing surface */
	sur = cairo_xcb_surface_create(conn, win, visual, w, h);
	if (cairo_surface_status(sur) != 0)
		die("unable to create Cairo surface\n");
	cr = cairo_create(sur);
}

static void text_update(xcb_key_press_event_t *e)
{
	xcb_keysym_t keysym;

	if (menus[menu].mode != MODE_PIN && menus[menu].mode != MODE_AMOUNT)
		return;

	keysym = xcb_key_symbols_get_keysym(keysyms, e->detail, 0);

	if (bufi == TEXT_BUFFER)
		return;

	switch (keysym) {
	case XK_0:
	case XK_KP_0:
	case XK_KP_Insert:
		buf[bufi++] = '0';
		break;
	case XK_1:
	case XK_KP_1:
	case XK_KP_End:
		buf[bufi++] = '1';
		break;
	case XK_2:
	case XK_KP_2:
	case XK_KP_Down:
		buf[bufi++] = '2';
		break;
	case XK_3:
	case XK_KP_3:
	case XK_KP_Page_Down:
		buf[bufi++] = '3';
		break;
	case XK_4:
	case XK_KP_4:
	case XK_KP_Left:
		buf[bufi++] = '4';
		break;
	case XK_5:
	case XK_KP_5:
	case XK_KP_Begin:
		buf[bufi++] = '5';
		break;
	case XK_6:
	case XK_KP_6:
	case XK_KP_Right:
		buf[bufi++] = '6';
		break;
	case XK_7:
	case XK_KP_7:
	case XK_KP_Home:
		buf[bufi++] = '7';
		break;
	case XK_8:
	case XK_KP_8:
	case XK_KP_Up:
		buf[bufi++] = '8';
		break;
	case XK_9:
	case XK_KP_9:
	case XK_KP_Page_Up:
		buf[bufi++] = '9';
		break;
	case XK_BackSpace:
	case XK_KP_Decimal:
	case XK_KP_Delete:
		buf[bufi++] = '<';
		break;
	case XK_Return:
	case XK_KP_Enter:
		buf[bufi++] = '>';
		break;
	default:
		return;
	}

	draw_menu();
}

static void font_init(void)
{
	if (!(font = pango_font_description_new()))
		die("unable to allocate new font\n");

	pango_font_description_set_family_static(font, font_desc);
}

static void font_fini(void)
{
	pango_font_description_free(font);
}

static xcb_atom_t atom_add(const char *name)
{
	return xcb_intern_atom_reply(conn, xcb_intern_atom(conn, 0,
				strlen(name), name), 0)->atom;
}

static void draw_init(void)
{
	const char *title = "kech";
	const uint32_t events = {
		XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_KEY_PRESS
	};

	if (!(conn = xcb_connect(0, 0)))
		die("unable to open display\n");

	if (!(screen = xcb_setup_roots_iterator(xcb_get_setup(conn)).data))
		die("unable to open screen\n");

	if (!keysyms)
		keysyms = xcb_key_symbols_alloc(conn);

	atoms[_NET_WM_NAME] = atom_add("_NET_WM_NAME");
	atoms[_NET_WM_STATE] = atom_add("_NET_WM_STATE");
	atoms[_NET_WM_STATE_FULLSCREEN] = atom_add("_NET_WM_STATE_FULLSCREEN");

	/* Create window */
	win = xcb_generate_id(conn);
	xcb_create_window(conn, screen->root_depth, win, screen->root, 0, 0,
			WIDTH, HEIGHT, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
			screen->root_visual, XCB_CW_EVENT_MASK, &events);

	/* Set title */
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
			atoms[_NET_WM_NAME], XCB_ATOM_STRING, 8, strlen(title),
			title);

	/* Hint for fullscreen */
	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
			atoms[_NET_WM_STATE], XCB_ATOM_ATOM, 32, 1,
			(const char *) &atoms[_NET_WM_STATE_FULLSCREEN]);

	/* Finally, map the window */
	xcb_map_window(conn, win);
	xcb_flush(conn);

	font_init();
}

void draw_fini(void)
{
	font_fini();

	xcb_flush(conn);
	xcb_disconnect(conn);
}

void *draw()
{
	xcb_generic_event_t *e;

	draw_init();

	while ((e = xcb_wait_for_event(conn))) {
		switch (e->response_type & ~0x80) {
		case XCB_EXPOSE:
			/*
			 * We have to do this after the window manager has
			 * processed the fullscreen window hint.
			 */
			if (!w || !h)
				geom_update();

			draw_menu();

			break;
		case XCB_BUTTON_PRESS:

			break;
		case XCB_KEY_PRESS:
			text_update((xcb_key_press_event_t *) e);

			break;
		default:
			/* xcb_error(e); */
			break;
		}

		free(e);
	}

	draw_fini();

	pthread_exit(NULL);
}
