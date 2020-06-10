#include "pistachio.h"

#define TEXTBOX_LEN   1000
#define ERROR_MSG_LEN  200

#define SCREEN        0
#define FONT_NAME     "monospace"
#define SEARCH_SIZE   16.0
#define RESULTS_SIZE  12.0
#define ERROR_SIZE    14.0
#define FOREGROUND    0xfff8f8f8
#define BACKGROUND    0xe8303030
#define CARET         0xffe0e0e0
#define SELECTED      0xf0608040
#define ERROR_COLOR   0xf0ff8080
#define WINDOW_WIDTH  0.4
#define WINDOW_HEIGHT 0.3

#define FOLDER_PROGRAM   "thunar"
#define DEFAULT_PROGRAM  "xed"
#define SUDO_PROMPT      ""

bool render_glyphs(Font_Renders *renders, Screen_Info *dimensions, GUI_Settings *settings) {
	if (!render_font(
		dimensions,
		settings->search_font_size,
		false,
		settings->foreground,
		settings->background,
		renders->search_glyphs
	))
		return false;

	if (settings->results_font_size != settings->search_font_size) {
		if (!render_font(
			dimensions,
			settings->results_font_size,
			false,
			settings->foreground,
			settings->background,
			renders->results_glyphs
		))
			return false;
	}
	else {
		// Copy the bitmap pointers instead - this eliminates needing to render the same font twice
		memcpy(renders->results_glyphs, renders->search_glyphs, N_CHARS * sizeof(Glyph));
	}

	if (!render_font(
		dimensions,
		settings->results_font_size,
		false,
		settings->foreground,
		settings->selected_color,
		renders->sel_glyphs
	))
		return false;

	return render_font(
		dimensions,
		settings->error_font_size,
		true,
		settings->error_color,
		settings->background,
		renders->error_glyphs
	);
}

char *parse_command(char *textbox, Command_Settings *programs, char *error, int error_len) {
	int len = strlen(textbox);
	int second = find_word(textbox, 0, len);
	bool is_command =
		second || (textbox[0] != '/' && textbox[0] != '~');

	bool append_daemonic_symbol = true;

	if (is_command) {
		int name_len = second ? second - 1 : len;
		char name[name_len + 1];
		memcpy(name, textbox, name_len);
		name[name_len] = 0;

		char *msg;
		if (!find_program(name, &msg)) {
			snprintf(error, error_len, "%s%s", msg, name);
			return NULL;
		}
	}
	else {
		struct stat s;
		if (stat_ex(textbox, len, &s) != 0) {
			snprintf(error, error_len, "file/folder not found: %s", textbox);
			return NULL;
		}

		printf("%x\n", s.st_mode);

		// If this is a folder
		if ((s.st_mode & S_IFMT) == S_IFDIR)
			prepend_word(programs->folder_program, textbox);

		// Else if it's a file
		else if ((s.st_mode & S_IFMT) == S_IFREG) {
			prepend_word(programs->default_file_program, textbox);

			// If a user that doesn't own this file does not have write access
			if ((s.st_mode & S_IWOTH) == 0) {
				prepend_word(programs->sudo_program, textbox);
				//append_daemonic_symbol = false;
			}
		}
	}

	if (append_daemonic_symbol) {
		len = strlen(textbox);
		if (textbox[len-1] != '&' && len < TEXTBOX_LEN-3)
			strcpy(&textbox[len], " &");
	}

	puts(textbox);
	return textbox;
}

int main(int argc, char **argv) {
	GUI_Settings settings = {
		.screen            = SCREEN,
		.frac_w            = WINDOW_WIDTH,
		.frac_h            = WINDOW_HEIGHT,
		.font_name         = FONT_NAME,
		.search_font_size  = SEARCH_SIZE,
		.results_font_size = RESULTS_SIZE,
		.error_font_size   = ERROR_SIZE,
		.foreground        = FOREGROUND,
		.background        = BACKGROUND,
		.caret_color       = CARET,
		.selected_color    = SELECTED,
		.error_color       = ERROR_COLOR
	};

	Command_Settings programs = {
		.folder_program       = FOLDER_PROGRAM,
		.default_file_program = DEFAULT_PROGRAM,
		.sudo_program         = SUDO_PROMPT
	};

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
	Glyph results_glyphs[N_CHARS] = {0};
	Glyph sel_glyphs[N_CHARS] = {0};
	Glyph error_glyphs[N_CHARS] = {0};

	Font_Renders renders = {search_glyphs, results_glyphs, sel_glyphs, error_glyphs};
	render_glyphs(&renders, &dimensions, &settings);

	close_font();

	char textbox[TEXTBOX_LEN] = {0};
	char error_buf[ERROR_MSG_LEN] = {0};
	char *error_msg = NULL;
	char *command = NULL;

	while (!command) {
		int res = run_gui(&settings, &dimensions, &renders, textbox, TEXTBOX_LEN, error_msg);
		if (res == STATUS_EXIT)
			break;

		command = parse_command(textbox, &programs, error_buf, ERROR_MSG_LEN);
		error_msg = &error_buf[0];
	}

	close_display();

	if (command)
		system(command);

	return 0;
}
