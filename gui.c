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
	int *menu;
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

void draw_string(char *text, int length, int offset, int *cursor, int x, int y, Draw_Info *draw_ctx, Glyph *glyphs) {
	if (length < 1)
		length = strlen(text);

	int caret_y1 = y - FONT_HEIGHT(glyphs[0]) * ABOVE_CURSOR_RATIO;
	int caret_y2 = y + FONT_HEIGHT(glyphs[0]) * BELOW_CURSOR_RATIO;

	if (text) {
		for (int i = offset; i < length && x < draw_ctx->window_w - BORDER_PX; i++) {
			int idx = glyph_indexof(text[i]);
			if (idx < 0)
				continue;

			if (glyphs[idx].data)
				XPutImage(
					display, draw_ctx->window, draw_ctx->gc, (XImage*)&glyphs[idx].ximage,
					0, 0,
					x + glyphs[idx].left, y - glyphs[idx].top,
					glyphs[idx].img_w, glyphs[idx].img_h
				);

			if (cursor && *cursor == i)
				XDrawLine(display, draw_ctx->window, draw_ctx->gc, x, caret_y1, x, caret_y2);

			x += FONT_WIDTH(glyphs[0]);
		}
	}

	if (cursor && *cursor == length)
		XDrawLine(display, draw_ctx->window, draw_ctx->gc, x, caret_y1, x, caret_y2);
}

void draw_menu(Menu_View *view, Listing *list, Settings *config, Glyph *renders, Draw_Info *draw_ctx, int y) {
	int results_font_h = FONT_HEIGHT(renders[RES_OFFSET]);
	int sel_offset = results_font_h * BELOW_CURSOR_RATIO;

	view->visible = (draw_ctx->window_h - BORDER_PX - y) / results_font_h + 1;

	for (int i = view->top; i < view->top + view->visible && i < view->n_items; i++) {
		int idx = view->menu[i];
		char *entry = list->table[idx];
		int len = strlen(entry);

		int offset = 0;
		int type = list->stats[idx].st_mode & S_IFMT;
		if (type == S_IFDIR)
			offset = 2 * N_CHARS;
		if (type == S_IFLNK)
			offset += N_CHARS;

		if (i == view->selected) {
			XSetForeground(display, draw_ctx->gc, config->selected_color);
			XFillRectangle(
				display, draw_ctx->window, draw_ctx->gc,
				BORDER_PX/2, y - results_font_h + sel_offset,
				draw_ctx->window_w - BORDER_PX, results_font_h
			);
			XSetForeground(display, draw_ctx->gc, config->caret_color);
			draw_string(entry, len, 0, NULL, BORDER_PX, y, draw_ctx, &renders[SEL_OFFSET + offset]);
		}
		else
			draw_string(entry, len, 0, NULL, BORDER_PX, y, draw_ctx, &renders[RES_OFFSET + offset]);

		y += results_font_h;
	}
}

int run_gui(Settings *config, Screen_Info *screen_info, Glyph *renders, char *textbox, int textbox_len, char *error_msg) {
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

	for (int i = 0; i < N_RENDERS; i++) {
		if (renders[i].data)
			make_32bpp_ximage(
				display, draw_ctx.visual,
				renders[i].data, renders[i].img_w, renders[i].img_h,
				(XImage*)&renders[i].ximage
			);
	}

	Atom delete_msg = XInternAtom(display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(display, draw_ctx.window, &delete_msg, 1);

	XMapRaised(display, draw_ctx.window);

	skip_taskbar(display, draw_ctx.window, RootWindow(display, screen_info->idx));

	int cursor = 0;

	int menu[MENU_SIZE] = {0};
	Menu_View view = {
		.menu = menu,
		.n_items = 0,
		.selected = -1,
		.top = 0
	};

	Listing listing;

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
				int font_h = FONT_HEIGHT(renders[BAR_OFFSET]);
				int y = font_h * VERT_GAP_RATIO;
				int caret_y1 = y - font_h * ABOVE_CURSOR_RATIO;
				int caret_y2 = y + font_h * BELOW_CURSOR_RATIO;
				XDrawLine(display, draw_ctx.window, draw_ctx.gc, x, caret_y1, x, caret_y2);

				if (error_msg)
					draw_string(
						error_msg, strlen(error_msg),
						0, NULL,
						BORDER_PX, draw_ctx.window_h - BORDER_PX,
						&draw_ctx,
						&renders[ERR_OFFSET]
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

				int up_delta = 0;
				int down_delta = 0;

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
						up_delta = 1;
						break;
					case XK_Page_Up:
						up_delta = view.visible;
						break;

					case XK_Down:
						down_delta = 1;
						break;
					case XK_Page_Down:
						down_delta = view.visible;
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

						view.top = 0;
						view.selected = -1;
						break;
				}
				if (done)
					break;

				if (up_delta) {
					int pos = view.selected - up_delta;
					view.selected = pos > -1 ? pos : -1;
					if (view.selected >= 0 && view.selected < view.top)
						view.top = view.selected;
					if (view.selected < 0)
						view.top = 0;
				}
				else if (down_delta) {
					int pos = view.selected + down_delta;
					view.selected = pos < view.n_items-1 ? pos : view.n_items-1;
					if (view.selected > view.top + view.visible-1)
						view.top = view.selected - (view.visible-1);
				}

				int add_len = 0;

				// shift+tab or modifier+left
				if (len > 0 && (key == XK_ISO_Left_Tab || (key == XK_Left && modifier_held))) {
					int idx = len - 1;
					do {
						textbox[idx] = 0;
						idx--;
					} while (idx >= 0 && textbox[idx] != '/');
					add_len = idx - (len-1);
				}

				if (len < textbox_len - input_len)
					add_len += insert_chars(textbox, len, key_buf, input_len, cursor);

				if (add_len) {
					view.top = 0;
					view.selected = -1;
					cursor += add_len;
				}

				char *word = NULL;
				int word_len = 0;
				int trailing = 0;
				memset(&listing, 0, sizeof(Listing));
				bool is_command = enumerate_directory(textbox, cursor, &word, &word_len, &trailing, &listing);

				char *match = NULL;
				int match_len = 0;

				if (key == XK_Tab && view.selected < 0) {
					match = find_completeable_span(&listing, word, word_len, trailing, &match_len);
				}
				else if (view.selected >= 0 && (key == XK_Tab || key == XK_Right || key == XK_Return) && listing.n_entries > 0) {
					match = listing.table[view.menu[view.selected]];
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
						enumerate_directory(textbox, cursor, &word, &word_len, NULL, &listing);

					cursor = &word[word_len] - textbox;
					view.selected = -1;
					view.top = 0;
				}

				view.n_items = 0;
				bool show_menu = listing.n_entries && !(is_command && trailing == 0);

				if (trailing == 0 && listing.n_entries > 0) {
					view.menu = listing.index;
					view.n_items = listing.n_entries;
				}
				else {
					view.menu = menu;
					if (show_menu) {
						for (int i = 0; i < listing.n_entries && view.n_items < MENU_SIZE; i++) {
							int idx = listing.index[i];
							if (!difference_ignoring_backslashes(listing.table[idx], word, word_len, trailing))
								view.menu[view.n_items++] = idx;
						}
					}
				}

				XClearArea(display, draw_ctx.window, 0, 0, draw_ctx.window_w, draw_ctx.window_h, false);

				int search_font_h = FONT_HEIGHT(renders[BAR_OFFSET]);
				int gap = search_font_h * VERT_GAP_RATIO;

				int max_chars = (draw_ctx.window_w - BORDER_PX) / FONT_WIDTH(renders[BAR_OFFSET]);
				int offset = (cursor >= max_chars) ? cursor - (max_chars-1) : 0;

				draw_string(textbox, -1, offset, &cursor, BORDER_PX, gap, &draw_ctx, &renders[BAR_OFFSET]);

				if (show_menu)
					draw_menu(&view, &listing, config, renders, &draw_ctx, gap * 2);

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
