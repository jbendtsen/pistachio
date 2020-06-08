#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "pistachio.h"

#define WINDOW_TITLE "Launcher"

#define ABOVE_CURSOR_RATIO  9/10
#define BELOW_CURSOR_RATIO  2/10
#define VERT_GAP_RATIO       4/3

#define BORDER_PX  10

#define MENU_SIZE 100

typedef struct {
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
 	image->data = data;

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

Window create_window(GUI_Settings *settings, Screen_Info *screen_info, char *title, Draw_Info *draw_info) {
	XVisualInfo info;
	XMatchVisualInfo(display, screen_info->idx, 32, TrueColor, &info);

	XSetWindowAttributes attr;
	attr.colormap = XCreateColormap(display, DefaultRootWindow(display), info.visual, AllocNone);
	attr.border_pixel = 0;
	attr.background_pixel = settings->background;

	int w = (float)screen_info->w * settings->frac_w;
	int h = (float)screen_info->h * settings->frac_h;
	int x = (screen_info->w - w) / 2;
	int y = (screen_info->h - h) / 2;

	draw_info->visual   = info.visual;
	draw_info->colormap = attr.colormap;
	draw_info->depth    = info.depth;
	draw_info->window_w = w;
	draw_info->window_h = h;

	Window window = XCreateWindow(
		display, DefaultRootWindow(display),
		x, y, w, h,
		0, info.depth, InputOutput, info.visual,
		CWColormap | CWBorderPixel | CWBackPixel, &attr
	);

	XStoreName(display, window, title);
	return window;
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
	Atom hint_msg = XInternAtom(display, "_MOTIF_WM_HINTS", 0);
	XChangeProperty(display, window, hint_msg, hint_msg, 32, PropModeReplace, (u8*)&hints, 5);
}

void draw_string(char *text, int length, int *cursor, int x, int y, Window window, GC gc, Glyph *glyphs, XImage *chars) {
	int caret_y1 = y - FONT_HEIGHT(glyphs) * ABOVE_CURSOR_RATIO;
	int caret_y2 = y + FONT_HEIGHT(glyphs) * BELOW_CURSOR_RATIO;

	if (text) {
		for (int i = 0; i < length; i++) {
			int idx = glyph_indexof(text[i]);
			if (idx < 0)
				continue;

			if (glyphs[idx].data)
				XPutImage(
					display, window, gc, &chars[idx],
					0, 0,
					x + glyphs[idx].left, y - glyphs[idx].top,
					glyphs[idx].img_w, glyphs[idx].img_h
				);

			if (cursor && *cursor == i)
				XDrawLine(display, window, gc, x, caret_y1, x, caret_y2);

			x += FONT_WIDTH(glyphs);
		}
	}

	if (cursor && *cursor == length)
		XDrawLine(display, window, gc, x, caret_y1, x, caret_y2);
}

void draw_menu(Menu_View *view, GUI_Settings *settings, Font_Renders *renders, Draw_Info *info, Window window, GC gc, int y) {
	int results_font_h = FONT_HEIGHT(renders->results_glyphs);
	int sel_offset = results_font_h * BELOW_CURSOR_RATIO;

	view->visible = (info->window_h - BORDER_PX - y) / results_font_h + 1;

	for (int i = view->top; i < view->top + view->visible && i < view->n_items; i++) {
		int len = strlen(view->menu[i]);
		if (i == view->selected) {
			XSetForeground(display, gc, settings->selected);
			XFillRectangle(
				display, window, gc,
				BORDER_PX/2, y - results_font_h + sel_offset,
				info->window_w - BORDER_PX, results_font_h
			);
			XSetForeground(display, gc, settings->caret);
			draw_string(view->menu[i], len, NULL, BORDER_PX, y, window, gc, renders->sel_glyphs, sel_chars);
		}
		else
			draw_string(view->menu[i], len, NULL, BORDER_PX, y, window, gc, renders->results_glyphs, results_chars);

		y += results_font_h;
	}
}

int run_gui(GUI_Settings *settings, Screen_Info *screen_info, Font_Renders *renders, char *textbox, int textbox_len) {
	Draw_Info info;
	Window window = create_window(settings, screen_info, WINDOW_TITLE, &info);

	remove_window_border(display, window);

	XSelectInput(display, window, ExposureMask | FocusChangeMask | ButtonPressMask | KeyPressMask);
	GC gc = XCreateGC(display, window, 0, NULL);

	XSetForeground(display, gc, settings->caret);
	XSetBackground(display, gc, settings->background);

	make_glyph_ximages(info.visual, renders->search_glyphs, &search_chars[0]);
	make_glyph_ximages(info.visual, renders->results_glyphs, &results_chars[0]);
	make_glyph_ximages(info.visual, renders->sel_glyphs, &sel_chars[0]);

	Atom delete_msg = XInternAtom(display, "WM_DELETE_WINDOW", false);
	XSetWMProtocols(display, window, &delete_msg, 1);

	XMapRaised(display, window);

	int cursor = 0;

	char *menu[MENU_SIZE] = {NULL};
	Menu_View view = {
		.menu = menu,
		.n_items = 0,
		.selected = -1,
		.top = 0
	};

	char key_buf[10] = {0};
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
				XDrawLine(display, window, gc, x, caret_y1, x, caret_y2);
				break;
			}
			case ButtonPress:
				break;

			case KeyPress:
			{
				KeySym key;
				int input_len = XLookupString((XKeyEvent*)&event, key_buf, 10, &key, 0);

				int len = strlen(textbox);
				bool draw = true;

				switch (key) {
					case XK_Escape:
						if (view.selected < 0)
							done = true;
						else {
							view.top = 0;
							view.selected = -1;
						}
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
						break;

					case XK_Delete:
						remove_char(textbox, len, cursor + 1);
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

				if (key == XK_Tab && view.selected < 0) {
					trailing = auto_complete(word, &word_len, word - textbox + textbox_len, results, n_results, trailing);
					if (!trailing)
						enumerate_directory(textbox, cursor, &word, &word_len, NULL, &results, &n_results);

					cursor = word - textbox + word_len;
				}

				if (view.selected >= 0 && (key == XK_Tab || key == XK_Right || key == XK_Return)) {
					char *buf = &menu[view.selected][trailing];
					word_len += insert_substring(word, len - (textbox - word), buf, strlen(buf), word_len);

					if (key == XK_Return) {
						done = true;
						run_command = true;
						break;
					}

					if (is_dir(word, word_len)) {
						trailing = 0;
						if (word[word_len-1] != '/')
							insert_substring(word, strlen(word), "/", 1, word_len);

						enumerate_directory(textbox, cursor, &word, &word_len, NULL, &results, &n_results);
					}

					cursor = &word[word_len] - textbox;
					view.selected = -1;
					view.top = 0;
				}

				view.n_items = 0;
				bool show_menu = results && n_results && !(is_command && trailing == 0);
				if (show_menu) {
					char *str = results;
					for (int i = 0; i < n_results && view.n_items < MENU_SIZE; i++, str += strlen(str) + 1) {
						if (!trailing || !strncmp(str, &word[word_len - trailing], trailing))
							view.menu[view.n_items++] = str;
					}
				}

				XClearArea(display, window, 0, 0, info.window_w, info.window_h, false);

				int search_font_h = FONT_HEIGHT(renders->search_glyphs);
				int gap = search_font_h * VERT_GAP_RATIO;

				len = strlen(textbox);
				draw_string(textbox, len, &cursor, BORDER_PX, gap, window, gc, renders->search_glyphs, search_chars);

				if (show_menu)
					draw_menu(&view, settings, renders, &info, window, gc, gap * 2);

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
		}
	}

	XFreeGC(display, gc);
	XDestroyWindow(display, window);

	return run_command ? STATUS_COMMAND : STATUS_EXIT;
}

bool open_display(int screen_idx, Screen_Info *screen_info) {
	display = XOpenDisplay(NULL);
	if (!display)
		return false;

	atexit(close_display);
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
