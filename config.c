#include "pistachio.h"

#define CONFIG_FILE   "~/.config/pistachio/configuration"

#define FONT_PATH        "/usr/share/fonts/noto/NotoSansMono-Regular.ttf"
#define SEARCH_SIZE      16.0
#define RESULTS_SIZE     12.5
#define ERROR_SIZE       14.0
#define FOREGROUND       0xfff8f8f8
#define BACKGROUND       0xe8303030
#define CARET            0xffe0e0e0
#define SELECTED         0xf0608040
#define ERROR_COLOR      0xf0ff8080
#define WINDOW_WIDTH     0.4
#define WINDOW_HEIGHT    0.3
#define FOLDER_PROGRAM   "thunar"
#define DEFAULT_PROGRAM  "xed"

#define POOL_SIZE 16 * 1024

static Arena arena = {0};

Settings config = {
	.window_w        = WINDOW_WIDTH,
	.window_h        = WINDOW_HEIGHT,
	.font_path       = FONT_PATH,
	.search_font = {
		.size    = SEARCH_SIZE,
		.color   = FOREGROUND,
		.oblique = false
	},
	.results_font = {
		.size    = RESULTS_SIZE,
		.color   = FOREGROUND,
		.oblique = false
	},
	.error_font = {
		.size    = ERROR_SIZE,
		.color   = ERROR_COLOR,
		.oblique = true
	},
	.back_color      = BACKGROUND,
	.caret_color     = CARET,
	.selected_color  = SELECTED,
	.folder_program = {
		FOLDER_PROGRAM,
		NULL,
		0,
		false,
		NULL
	},
	.default_program = {
		DEFAULT_PROGRAM,
		NULL,
		0,
		false,
		NULL
	}
};

char *command_list[] = {
	"font-path",
	"search-font",
	"results-font",
	"error-font",
	"back-color",
	"caret-color",
	"hl-color",
	"window-width",
	"window-height",
	"folder-command",
	"default-command",
	"program",
	"command",
	"nodaemon"
};

char *allocate_string(char *src, int len) {
	char *str = allocate(&arena, len + 1);
	memcpy(str, src, len);
	str[len] = 0;
	return str;
}

int set_color(char *params, int params_len, int offset, u32 *out_color) {
	char *p = &params[offset];
	int idx = 0;
	u32 color = 0;

	int i;
	for (i = offset; i < params_len && idx < 8; i++, p++) {
		if (*p == ' ' || *p == '\t')
			break;

		if (*p >= '0' && *p <= '9')
			color = (color << 4) | (*p - '0');
		else if (*p >= 'A' && *p <= 'F')
			color = (color << 4) | (*p - 'A' + 0xa);
		else if (*p >= 'a' && *p <= 'f')
			color = (color << 4) | (*p - 'a' + 0xa);
		else
			idx--;
		idx++;
	}

	if (idx == 8 || color > 0xFFFFFF)
		*out_color = color;

	return i - offset;
}

void set_font_attrs(char *params, int params_len, Font_Attrs *font) {
	if (!params_len)
		return;

	// find the end of the first word
	char *p = params;
	char *word = p;
	while ((p-params) < params_len && *p != ' ' && *p != '\t') p++;

	char *size_str = allocate_string(word, p-word);
	double size = atof(size_str);
	if (size > 0.0)
		font->size = (float)size;

	// skip to the next word
	while ((p-params) < params_len && (*p == ' ' || *p == '\t')) p++;

	p += set_color(params, p-params, params_len, &font->color);

	while ((p-params) < params_len && (*p == ' ' || *p == '\t')) p++;

	font->oblique = !strncmp(p, "oblique", 7);
}

// If size >= 1.0, then size is a percentage of a screen dimension.
// Else size is in pixels.
void set_size(char *params, int params_len, float *size) {
	if (params_len < 2)
		return;

	char *str = allocate_string(params, params_len);
	if (params[params_len-1] == '%')
		*size = (float)(atof(str) / 100.0);
	else
		*size = (float)atof(str);
}

void add_program(char *command, char *extensions, int n_extensions, bool daemonize) {
	Program **head = &config.programs;
	while (*head)
		head = &(*head)->next;

	Program *prog = allocate(&arena, sizeof(Program));
	*prog = (Program) {
		.command = command,
		.extensions = extensions,
		.n_extensions = n_extensions,
		.daemonize = daemonize,
		.next = NULL
	};

	*head = prog;
}

void find_first_two_words(char *params, int params_len, int *first_len, int *second) {
	// find the end of the first word
	char *p = params;
	while ((p-params) < params_len && *p != ' ' && *p != '\t') p++;
	*first_len = p-params;

	// skip to the next word
	while ((p-params) < params_len && (*p == ' ' || *p == '\t')) p++;
	*second = p-params;
}

void set_program(char *params, int params_len, bool daemonize) {
	if (!params_len)
		return;

	int cmd_len, exts_offset;
	find_first_two_words(params, params_len, &cmd_len, &exts_offset);

	if (exts_offset >= params_len)
		return;

	char *cmd = allocate_string(params, cmd_len);

	int exts_size = params_len - exts_offset + 1;
	char *exts = allocate(&arena, exts_size);
	memset(exts, 0, exts_size);

	int n_exts = 1;
	bool was_blank = false;
	char *src = &params[exts_offset];
	char *dst = exts;

	for (int i = exts_offset; i < params_len; i++) {
		if (*src != ' ' && *src != '\t') {
			if (was_blank) {
				n_exts++;
				dst++;
			}
			*dst++ = *src++;
			was_blank = false;
		}
		else {
			src++;
			was_blank = true;
		}
	}

	add_program(cmd, exts, n_exts, daemonize);
}

void set_command(char *params, int params_len, bool daemonize) {
	if (!params_len)
		return;

	int ext_len, cmd_offset;
	find_first_two_words(params, params_len, &ext_len, &cmd_offset);

	if (cmd_offset >= params_len)
		return;

	char *ext = allocate_string(params, ext_len);
	char *cmd = allocate_string(&params[cmd_offset], params_len - cmd_offset);

	add_program(cmd, ext, 1, daemonize);
}

void parse_config_line(char *line, int len, bool *daemonize) {
	int idx = -1;
	int cmd_len = 0;
	for (int i = 0; i < sizeof(command_list) / sizeof(char*); i++) {
		cmd_len = strlen(command_list[i]);
		if (!strncmp(command_list[i], line, cmd_len)) {
			idx = i;
			break;
		}
	}

	if (idx < 0)
		return;

	// constrict the line length to before the trailing whitespace (if there is any)
	char *p = &line[len-1];
	while (*p && (*p == ' ' || *p == '\t')) p--;
	len = p - line + 1;

	// find the beginning of the parameters
	char *params = &line[cmd_len];
	int params_len = len - cmd_len;
	for (
		int i = cmd_len;
		i < len && (*params == ' ' || *params == '\t');
		i++, params++, params_len--
	);

	switch (idx) {
		case 0: // font-name
			config.font_path = allocate_string(params, params_len);
			break;

		case 1: // search-font
			set_font_attrs(params, params_len, &config.search_font);
			break;
		case 2: // results-font
			set_font_attrs(params, params_len, &config.results_font);
			break;
		case 3: // error-font
			set_font_attrs(params, params_len, &config.error_font);
			break;

		case 4: // back-color
			set_color(params, params_len, 0, &config.back_color);
			break;
		case 5: // caret-color
			set_color(params, params_len, 0, &config.caret_color);
			break;
		case 6: // hl-color
			set_color(params, params_len, 0, &config.selected_color);
			break;

		case 7: // window-width
			set_size(params, params_len, &config.window_w);
			break;
		case 8: // window-height
			set_size(params, params_len, &config.window_h);
			break;

		case 9: // folder-command
			config.folder_program.command = allocate_string(params, params_len);
			config.folder_program.daemonize = daemonize != NULL ? *daemonize : true;
			break;
		case 10: // default-command
			config.default_program.command = allocate_string(params, params_len);
			config.default_program.daemonize = daemonize != NULL ? *daemonize : true;
			break;

		case 11: // program
			set_program(params, params_len, daemonize != NULL ? *daemonize : true);
			break;
		case 12: // command
			set_command(params, params_len, daemonize != NULL ? *daemonize : true);
			break;

		case 13: // nodaemon
		{
			bool d = false;
			parse_config_line(params, params_len, &d);
			break;
		}
	}
}

Settings *load_config() {
	if (!arena.initialized)
		make_arena(POOL_SIZE, &arena);

	char *path = get_desugared_path(CONFIG_FILE, strlen(CONFIG_FILE));

	FILE *f = fopen(path, "rb");
	if (!f) {
		save_config(path);
		return &config;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	char *file = allocate(&arena, sz + 1);
	fread(file, 1, sz, f);
	file[sz] = 0;
	fclose(f);

	char *p = file;
	while (*p) {
		// skip leading whitespace
		while (*p && (*p == ' ' || *p == '\t')) p++;
		bool comment = *p == '#';

		// find the last viable character
		char *line = p;
		while (*p && *p != '#' && *p != '\n' && *p != '\r') p++;

		int len = p - line;

		// skip to the end of the line if we found a comment
		if (*p == '#')
			while (*p && *p != '\n') p++;

		// skip to the next viable line
		while (*p && (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t')) p++;

		if (comment)
			continue;

		parse_config_line(line, len, NULL);
	}

	if (config.font_path[0] == '~')
		config.font_path = get_desugared_path(config.font_path, strlen(config.font_path));

	return &config;
}

void save_config(char *cfg_path) {
	int len = strlen(cfg_path);
	char *path = allocate(&arena, len + 1);
	memset(path, 0, len + 1);

	char *src = cfg_path, *dst = path;
	for (int i = 0; i < len; i++) {
		if (i > 0 && *src == '/')
			mkdir(path, 0777);

		*dst++ = *src++;
	}

	FILE *f = fopen(path, "w");
	if (!f) {
		fprintf(stderr, "Could not write new configuration file \"%s\"\n", path);
		return;
	}

	u32 colors[] = {
		config.search_font.color,
		config.results_font.color,
		config.error_font.color,
		config.back_color,
		config.caret_color,
		config.selected_color
	};
	char color_strs[54] = {0};

	char *p = color_strs;
	for (int i = 0; i < 6; i++) {
		u32 c = colors[i];
		for (int j = 0; j < 8; j++) {
			u32 d = (c >> 28) & 0xf;
			*p++ = d < 10 ? '0' + d : 'a' + (d-10);
			c <<= 4;
		}
		p++;
	}

	fprintf(f,
		"font-path %s\n"
		"search-font %g %s%s\n"
		"results-font %g %s%s\n"
		"error-font %g %s%s\n"
		"back-color %s\n"
		"caret-color %s\n"
		"hl-color %s\n"
		"window-width %g%%\n"
		"window-height %g%%\n"
		"folder-program %s\n"
		"default-program %s\n",
		config.font_path,
		config.search_font.size,  &color_strs[0 * 9], config.search_font.oblique ? " oblique" : "",
		config.results_font.size, &color_strs[1 * 9], config.results_font.oblique ? " oblique" : "",
		config.error_font.size,  &color_strs[2 * 9], config.error_font.oblique ? " oblique" : "",
		&color_strs[3 * 9],
		&color_strs[4 * 9],
		&color_strs[5 * 9],
		config.window_w * 100.0,
		config.window_h * 100.0,
		config.folder_program.command,
		config.default_program.command
	);

	fclose(f);
}
