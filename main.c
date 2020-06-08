#include "pistachio.h"

#define TEXTBOX_LEN   1000

#define SCREEN        0
#define FONT_NAME     "monospace"
#define SEARCH_SIZE   16
#define RESULTS_SIZE  12
#define FOREGROUND    0xfff8f8f8
#define BACKGROUND    0xe8303030
#define CARET         0xffe0e0e0
#define SELECTED      0xf0508040
#define WINDOW_WIDTH  0.4
#define WINDOW_HEIGHT 0.3

int main(int argc, char **argv) {
	GUI_Settings settings = {
		.screen            = SCREEN,
		.frac_w            = WINDOW_WIDTH,
		.frac_h            = WINDOW_HEIGHT,
		.font_name         = FONT_NAME,
		.search_font_size  = SEARCH_SIZE,
		.results_font_size = RESULTS_SIZE,
		.foreground        = FOREGROUND,
		.background        = BACKGROUND,
		.caret             = CARET,
		.selected          = SELECTED
	};
	make_argb(settings.foreground, &settings.fore_argb);
	make_argb(settings.background, &settings.back_argb);
	make_argb(settings.caret, &settings.caret_argb);
	make_argb(settings.selected, &settings.sel_argb);

	char *font_path = get_font_path(settings.font_name);
	if (!font_path) {
		fprintf(stderr, "Font error: Could not find \"%s\"\n", FONT_NAME);
		return 1;
	}

	Screen_Info dimensions;
	if (!open_display(settings.screen, &dimensions)) {
		fprintf(stderr, "Failed to access the display\n");
		return 2;
	}

	if (!open_font(font_path))
		return 3;

	Glyph search_glyphs[N_CHARS] = {0};
	if (!render_font(&dimensions, settings.search_font_size, &settings.fore_argb, &settings.back_argb, &search_glyphs[0]))
		return 4;

	Glyph results_glyphs[N_CHARS] = {0};
	if (settings.results_font_size != settings.search_font_size) {
		if (!render_font(&dimensions, settings.results_font_size, &settings.fore_argb, &settings.back_argb, &results_glyphs[0]))
			return 5;
	}
	else
		memcpy(&results_glyphs[0], &search_glyphs[0], N_CHARS * sizeof(Glyph));

	Glyph sel_glyphs[N_CHARS] = {0};
	if (!render_font(&dimensions, settings.results_font_size, &settings.fore_argb, &settings.sel_argb, &sel_glyphs[0]))
		return 6;

	Font_Renders renders = {
		&search_glyphs[0],
		&results_glyphs[0],
		&sel_glyphs[0]
	};

	close_font();

	char textbox[TEXTBOX_LEN] = {0};
	if (run_gui(&settings, &dimensions, &renders, textbox, TEXTBOX_LEN) == STATUS_COMMAND) {
		close_display();

		int len = strlen(textbox);
		if (textbox[len-1] != '&' && len < TEXTBOX_LEN-3)
			strcpy(textbox + len, " &");

		system(textbox);
	}

	return 0;
}
