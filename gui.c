#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "pistachio.h"

#define WINDOW_TITLE "pistachio"

#define ABOVE_CURSOR_RATIO  9/10
#define BELOW_CURSOR_RATIO  2/10
#define VERT_GAP_RATIO       4/3

#define BORDER_PX  10

#define MENU_SIZE 1024

typedef struct {
	Window window;
	GC gc;
	Visual *visual;
	Colormap colormap;
	int depth;
	int window_w;
	int window_h;
} Draw_Info;

typedef struct {
	char **menu;
	int n_items;
	int selected;
	int top;
	int visible;
} Menu_View;

XImage search_chars[N_CHARS] = {0};
XImage results_chars[N_CHARS] = {0};
XImage sel_chars[N_CHARS] = {0};
XImage error_chars[N_CHARS] = {0};

Display *display = NULL;

void make_32bpp_ximage(Display *dpy, Visual *visual, u8 *data, int w, int h, XImage *image) {
	image->width = w;
	image->height = h;
	image->format = ZPixmap;

	image->byte_order = ImageByteOrder(dpy);
	image->bitmap_unit = BitmapUnit(dpy);
	image->bitmap_bit_order = BitmapBitOrder(dpy);
	image->bitmap_pad = BitmapPad(dpy);

	image->red_mask = visual->red_mask;
	image->green_mask = visual->green_mask;
	image->blue_mask = visual->blue_mask;

	image->xoffset = 0;
	image->depth = 32;
 	image->data = (char*)data;

	image->bytes_per_line = w * 4;
	image->bits_per_pixel = 32;

	image->obdata = NULL;

	XInitImage(image);
}

void make_glyph_ximages(Visual *visual, Glyph *glyphs, XImage *images) {
	for (int i = 0; i < N_CHARS; i++) {
		if (glyphs[i].data)
			make_32bpp_ximage(display, visual, glyphs[i].data, glyphs[i].img_w, glyphs[i].img_h, &images[i]);
	}
}

void create_window(Settings *config, Screen_Info *screen_info, Draw_Info *draw_ctx) {
	XVisualInfo info;
	XMatchVisualInfo(display, screen_info->idx, 32, TrueColor, &info);

	XSetWindowAttributes attr;
	attr.colormap = XCreateColormap(display, DefaultRootWindow(display), info.visual, AllocNone);
	attr.border_pixel = 0;
	attr.background_pixel = config->back_color;
	attr.win_gravity = CenterGravity;

	int w = (float)screen_info->w * config->window_w;
	int h = (float)screen_info->h * config->window_h;

	draw_ctx->visual   = info.visual;
	draw_ctx->colormap = attr.colormap;
	draw_ctx->depth    = info.depth;
	draw_ctx->window_w = w;
	draw_ctx->window_h = h;

	draw_ctx->window = XCreateWindow(
		display, RootWindow(display, screen_info->idx),
		0, 0, w, h,
		0, info.depth, InputOutput, info.visual,
		CWColormap | CWBorderPixel | CWBackPixel | CWWinGravity,
		&attr
	);
}

void remove_window_border(Display *dpy, Window window) {
	struct {
		unsigned long flags;
		unsigned long functions;
		unsigned long decorations;
		long input_mode;
		unsigned long status;
	} hints = {
		.flags = (1L << 1), // decorations hint flag
		.decorations = 0
	};
	Atom hint_msg = XInternAtom(display, "_MOTIF_WM_HINTS", false);
	XChangeProperty(display, window, hint_msg, hint_msg, 32, PropModeReplace, (u8*)&hints, 5);
}

// This function lets us avoid having the window in the taskbar.
// Note: this function must be called AFTER XMapRaised()
void skip_taskbar(Display *dpy, Window window, Window root) {
	XClientMessageEvent xclient = {
		.type = ClientMessage,
		.window = window,
		.message_type = XInternAtom(display, "_NET_WM_STATE", false),
		.format = 32, // 32 bits per data member
		.data.l[0] = 1, // Add/set property
		.data.l[1] = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", false),
		.data.l[2] = 0, // Second property to set = None
		.data.l[3] = 1, // 1 for client apps, 2 for pager apps
		.data.l[4] = 0  // no more values
	};

	XSendEvent(
		dpy, root, false,
		SubstructureRedirectMask | SubstructureNotifyMask,
		(XEvent*)&xclient
	);
}

void draw_string(char *text, int length, int *cursor, int x, int y, Draw_Info *draw_ctx, Glyph *glyphs, XImage *chars) {
	if (length < 1)
		length = strlen(text);

	int caret_y1 = y - FONT_HEIGHT(glyphs) * ABOVE_CURSOR_RATIO;
	int caret_y2 = y + FONT_HEIGHT(glyphs) * BELOW_CURSOR_RATIO;

	if (text) {
		for (int i = 0; i < length && x < draw_ctx->window_w - BORDER_PX; i++) {
			int idx = glyph_indexof(text[i]);
			if (idx < 0)
				continue;

			if (glyphs[idx].data)
				XPutImage(
					display, draw_ctx->window, draw_ctx->gc, &chars[idx],
					0, 0,
					x + glyphs[idx].left, y - glyphs[idx].top,
					glyphs[idx].img_w, glyphs[idx].img_h
				);

			if (cursor && *cursor == i)
				XDrawLine(display, draw_ctx->window, draw_ctx->gc, x, caret_y1, x, caret_y2);

			x += FONT_WIDTH(glyphs);
		}
	}

	if (cursor && *cursor == length)
		XDrawLine(display, draw_ctx->window, draw_ctx->gc, x, caret_y1, x, caret_y2);
}

void draw_menu(Menu_View *view, Settings *config, Font_Renders *renders, Draw_Info *draw_ctx, int y) {
	int results_font_h = FONT_HEIGHT(renders->results_glyphs);
	int sel_offset = results_font_h * BELOW_CURSOR_RATIO;

	view->visible = (draw_ctx->window_h - BORDER_PX - y) / results_font_h + 1;

	for (int i = view->top; i < view->top + view->visible && i < view->n_items; i++) {
		int len = strlen(view->menu[i]);
		if (i == view->selected) {
			XSetForeground(display, draw_ctx->gc, config->selected_color);
			XFillRectangle(
				display, draw_ctx->window, draw_ctx->gc,
				BORDER_PX/2, y - results_font_h + sel_offset,
				draw_ctx->window_w - BORDER_PX, results_font_h
			);
			XSetForeground(display, draw_ctx->gc, config->caret_color);
			draw_string(view->menu[i], len, NULL, BORDER_PX, y, draw_ctx, renders->sel_glyphs, sel_chars);
		}
		else
			draw_string(view->menu[i], len, NULL, BORDER_PX, y, draw_ctx, renders->results_glyphs, results_chars);

		y += results_font_h;
	}
}

int run_gui(Settings *config, Screen_Info *screen_info, Font_Renders *renders, char *textbox, int textbox_len, char *error_msg) {
	if (error_msg) {
		XSync(display, true);
		XFlush(display);
		memset(textbox, 0, textbox_len);
	}

	Draw_Info draw_ctx;
	create_window(config, screen_info, &draw_ctx);

	XStoreName(display, draw_ctx.window, WINDOW_TITLE);
	remove_window_border(display, draw_ctx.window);

	XSelectInput(display, draw_ctx.window, ExposureMask | FocusChangeMask | KeyPressMask | KeyReleaseMask);
	draw_ctx.gc = XCreateGC(display, draw_ctx.window, 0, NULL);

	XSetForeground(display, draw_ctx.gc, config->caret_color);
	XSetBackground(display, draw_ctx.gc, config->back_color);

	make_glyph_ximages(draw_ctx.visual, renders->search_glyphs, search_chars);
	make_glyph_ximages(draw_ctx.visual, renders->results_glyphs, results_chars);
	make_glyph_ximages(draw_ctx.visual, renders->sel_glyphs, sel_chars);
	make_glyph_ximages(draw_ctx.visual, renders->error_glyphs, error_chars);

	Atom delete_msg = XInternAtom(display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(display, draw_ctx.window, &delete_msg, 1);

	XMapRaised(display, draw_ctx.window);

	skip_taskbar(display, draw_ctx.window, RootWindow(display, screen_info->idx));

	int cursor = 0;

	char *menu[MENU_SIZE] = {NULL};
	Menu_View view = {
		.menu = menu,
		.n_items = 0,
		.selected = -1,
		.top = 0
	};

	char key_buf[10] = {0};
	bool modifier_held = false;

	bool run_command = false;
	bool done = false;

	while (!done) {
		XEvent event;
		XNextEvent(display, &event);

		switch (event.type) {
			case Expose:
			{
				int x = BORDER_PX;
				int y = FONT_HEIGHT(renders->search_glyphs) * VERT_GAP_RATIO;
				int caret_y1 = y - FONT_HEIGHT(renders->search_glyphs) * ABOVE_CURSOR_RATIO;
				int caret_y2 = y + FONT_HEIGHT(renders->search_glyphs) * BELOW_CURSOR_RATIO;
				XDrawLine(display, draw_ctx.window, draw_ctx.gc, x, caret_y1, x, caret_y2);

				if (error_msg)
					draw_string(
						error_msg, strlen(error_msg),
						NULL,
						BORDER_PX, draw_ctx.window_h - BORDER_PX,
						&draw_ctx,
						renders->error_glyphs, error_chars
					);

				break;
			}
			case FocusOut:
				if (event.xfocus.mode != NotifyUngrab)
					done = true;
				break;

			case ClientMessage:
				if (event.xclient.data.l[0] == delete_msg)
					done = true;
				break;

			case KeyRelease:
				if (IsModifierKey(XLookupKeysym((XKeyEvent*)&event, 0)))
					modifier_held = false;
				break;

			case KeyPress:
			{
				int len = strlen(textbox);

				KeySym key;
				int input_len = XLookupString((XKeyEvent*)&event, key_buf, 10, &key, 0);
				if (IsModifierKey(key))
					modifier_held = true;

				switch (key) {
					case XK_Escape:
						done = true;
						break;

					case XK_Return:
						if (view.selected < 0) {
							done = true;
							run_command = true;
						}
						break;

					case XK_Up:
						view.selected = view.selected > -1 ? view.selected-1 : -1;
						if (view.selected >= 0 && view.selected < view.top)
							view.top = view.selected;
						break;

					case XK_Down:
						view.selected = view.selected < view.n_items-1 ? view.selected+1 : view.n_items-1;
						if (view.selected > view.top + view.visible-1)
							view.top = view.selected - (view.visible-1);
						break;

					case XK_Left:
						if (view.selected < 0)
							cursor = cursor > 0 ? cursor-1 : 0;
						break;

					case XK_Right:
						if (view.selected < 0)
							cursor = cursor < len ? cursor+1 : len;
						break;

					case XK_Home:
						cursor = 0;
						break;

					case XK_End:
						cursor = len;
						break;

					case XK_BackSpace:
						remove_char(textbox, len, cursor);
						if (cursor > 0) cursor--;
					case XK_Delete:
						if (key == XK_Delete)
							remove_char(textbox, len, cursor + 1);

						if (modifier_held) {
							memset(textbox, 0, textbox_len);
							cursor = len = 0;
						}
						view.top = 0;
						view.selected = -1;
						break;
				}
				if (done)
					break;

				int add_len = 0;
				if (len < textbox_len - input_len)
					add_len = insert_chars(textbox, len, key_buf, input_len, cursor);

				if (add_len) {
					view.top = 0;
					view.selected = -1;
					cursor += add_len;
				}

				int n_results = 0, trailing = 0;
				char *results = NULL;
				char *word = NULL;
				int word_len = 0;
				bool is_command = enumerate_directory(textbox, cursor, &word, &word_len, &trailing, &results, &n_results);

				char *match = NULL;
				int match_len = 0;

				if (key == XK_Tab && view.selected < 0) {
					match = find_completeable_span(word, word_len, results, n_results, trailing, &match_len);
				}
				else if (view.selected >= 0 && (key == XK_Tab || key == XK_Right || key == XK_Return)) {
					match = menu[view.selected];
					match_len = strlen(match);
				}

				if (match) {
					bool folder_completion = view.n_items == 1 || view.selected >= 0;
					trailing = complete(word, &word_len, match, match_len, trailing, folder_completion);

					if (key == XK_Return) {
						done = true;
						run_command = true;
						break;
					}

					if (!trailing)
						enumerate_directory(textbox, cursor, &word, &word_len, NULL, &results, &n_results);

					cursor = &word[word_len] - textbox;
					view.selected = -1;
					view.top = 0;
				}

				view.n_items = 0;
				bool show_menu = results && n_results && !(is_command && trailing == 0);
				if (show_menu) {
					char *str = results;
					for (int i = 0; i < n_results && view.n_items < MENU_SIZE; i++, str += strlen(str) + 1) {
						if (!difference_ignoring_backslashes(str, word, word_len, trailing))
							view.menu[view.n_items++] = str;
					}
				}

				XClearArea(display, draw_ctx.window, 0, 0, draw_ctx.window_w, draw_ctx.window_h, false);

				int search_font_h = FONT_HEIGHT(renders->search_glyphs);
				int gap = search_font_h * VERT_GAP_RATIO;

				draw_string(textbox, -1, &cursor, BORDER_PX, gap, &draw_ctx, renders->search_glyphs, search_chars);

				if (show_menu)
					draw_menu(&view, config, renders, &draw_ctx, gap * 2);

				break;
			}
		}
	}

	XFreeGC(display, draw_ctx.gc);
	XDestroyWindow(display, draw_ctx.window);

	return run_command ? STATUS_COMMAND : STATUS_EXIT;
}

bool open_display(int screen_idx, Screen_Info *screen_info) {
	display = XOpenDisplay(NULL);
	if (!display)
		return false;

	Screen *screen = ScreenOfDisplay(display, screen_idx);
	if (!screen)
		return false;

	screen_info->idx   = screen_idx;
	screen_info->w     = WidthOfScreen(screen);
	screen_info->h     = HeightOfScreen(screen);
	screen_info->dpi_w = (float)screen_info->w * 25.4 / (float)WidthMMOfScreen(screen);
	screen_info->dpi_h = (float)screen_info->h * 25.4 / (float)HeightMMOfScreen(screen);

	return true;
}

void close_display() {
	if (display) {
		XCloseDisplay(display);
		display = NULL;
	}
}
