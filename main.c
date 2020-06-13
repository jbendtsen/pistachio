#include "pistachio.h"

#define TEXTBOX_LEN    400
#define ERROR_MSG_LEN  400

#define SCREEN        0

bool render_glyphs(Font_Renders *renders, Screen_Info *dimensions, Settings *config) {
	if (!render_font(
		dimensions,
		&config->search_font,
		config->back_color,
		renders->search_glyphs
	))
		return false;

	if (!render_font(
		dimensions,
		&config->results_font,
		config->back_color,
		renders->results_glyphs
	))
		return false;

	if (!render_font(
		dimensions,
		&config->results_font,
		config->selected_color,
		renders->sel_glyphs
	))
		return false;

	return render_font(
		dimensions,
		&config->error_font,
		config->back_color,
		renders->error_glyphs
	);
}

char *parse_command(char *textbox, Settings *config, char *error, int error_len) {
	int len = strlen(textbox);
	int second = find_next_word(textbox, 0, len);
	bool is_command =
		second || (textbox[0] != '/' && textbox[0] != '~');

	bool daemonize = true;

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
		char *path = get_desugared_path(textbox, len);
		if (stat(path, &s) != 0) {
			snprintf(error, error_len, "file/folder not found: %s", textbox);
			return NULL;
		}

		// If this is a folder
		if ((s.st_mode & S_IFMT) == S_IFDIR) {
			prepend_word(config->folder_program.command, textbox);
			daemonize = config->folder_program.daemonize;
		}
		// Else if it's a file
		else if ((s.st_mode & S_IFMT) == S_IFREG) {
			Program *prog = config->programs;

			while (prog) {
				bool found = false;
				char *ext = prog->extensions;
				int ext_len = 0;

				for (int i = 0; i < prog->n_extensions; i++, ext += ext_len + 1) {
					ext_len = strlen(ext);
					if (!strcmp(&textbox[len - ext_len], ext)) {
						found = true;
						break;
					}
				}
				if (found)
					break;

				prog = prog->next;
			}
			if (!prog)
				prog = &config->default_program;

			prepend_word(prog->command, textbox);
			daemonize = prog->daemonize;
		}
	}

	if (daemonize) {
		len = strlen(textbox);
		if (textbox[len-1] != '&' && len < TEXTBOX_LEN-3)
			strcpy(&textbox[len], " &");
		else
			textbox[len-1] = 0;
	}

	return textbox;
}

int main(int argc, char **argv) {
	defer_arena_destruction();
	init_directory_arena();
	Settings *config = load_config();

	struct stat s = {0};
	if (stat(config->font_path, &s) != 0 || (s.st_mode & S_IFREG) == 0) {
		fprintf(stderr, "Could not find font (absolute path \"%s\" does not specify a file)\n", config->font_path);
		return 2;
	}

	Screen_Info dimensions;
	if (!open_display(SCREEN, &dimensions)) {
		fprintf(stderr, "Failed to access the display\n");
		return 3;
	}

	if (config->window_w > 1.0)
		config->window_w /= (float)dimensions.w;
	if (config->window_h > 1.0)
		config->window_h /= (float)dimensions.h;

	if (!open_font(config->font_path))
		return 4;

	Glyph search_glyphs[N_CHARS] = {0};
	Glyph results_glyphs[N_CHARS] = {0};
	Glyph sel_glyphs[N_CHARS] = {0};
	Glyph error_glyphs[N_CHARS] = {0};

	Font_Renders renders = {search_glyphs, results_glyphs, sel_glyphs, error_glyphs};
	render_glyphs(&renders, &dimensions, config);

	close_font();

	char textbox[TEXTBOX_LEN] = {0};
	char error_buf[ERROR_MSG_LEN] = {0};
	char *error_msg = NULL;
	char *command = NULL;

	while (!command) {
		int res = run_gui(config, &dimensions, &renders, textbox, TEXTBOX_LEN, error_msg);
		if (res == STATUS_EXIT)
			break;

		command = parse_command(textbox, config, error_buf, ERROR_MSG_LEN);
		error_msg = &error_buf[0];
	}

	close_display();

	if (command)
		system(command);

	return 0;
}
