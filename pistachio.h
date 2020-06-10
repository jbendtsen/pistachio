#ifndef PISTACHIO_H
#define PISTACHIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#define FONT_WIDTH(glyphs) (int)(glyphs[0].box_w + 0.5)
#define FONT_HEIGHT(glyphs) (int)(glyphs[0].box_h + 0.5)

#define MIN_CHAR ' '
#define MAX_CHAR '~'
#define N_CHARS  (MAX_CHAR - MIN_CHAR + 1)

#define BINARIES_DIR  "/usr/bin"

#define STATUS_EXIT     0
#define STATUS_COMMAND  1

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
	float a, r, g, b;
} ARGB;

typedef struct {
	int screen;
	float frac_w;
	float frac_h;
	char *font_name;
	float search_font_size;
	float results_font_size;
	float error_font_size;
	u32 foreground;
	u32 background;
	u32 caret_color;
	u32 selected_color;
	u32 error_color;
} GUI_Settings;

typedef struct {
	char *folder_program;
	char *default_file_program;
	char *sudo_program;
} Command_Settings;

typedef struct {
	int idx;
	int w;
	int h;
	int dpi_w;
	int dpi_h;
} Screen_Info;

typedef struct {
	u8 *data;
	int img_w, img_h;
	int pitch;
	float box_w, box_h;
	int left, top;
} Glyph;

typedef struct {
	Glyph *search_glyphs;
	Glyph *results_glyphs;
	Glyph *sel_glyphs;
	Glyph *error_glyphs;
} Font_Renders;

// utils.c
void make_argb(u32 color, ARGB *argb);
void remove_char(char *str, int len, int pos);
int insert_chars(char *str, int len, char *insert, int insert_len, int pos);
int insert_substring(char *str, int len, char *insert, int insert_len, int pos);
int remove_backslashes(char *str, int span);
int escape_spaces(char *str, int span);
int find_word(char *str, int start, int end);
void prepend_word(char *word, char *sentence);
bool difference_ignoring_backslashes(char *str, char *word, int word_len, int trailing);
bool enumerate_directory(char *textbox, int cursor, char **word, int *word_length, int *search_length, char **result, int *n_entries);
char *find_completeable_span(char *word, int word_len, char *results, int n_results, int trailing, int *match_length);
int complete(char *word, int *word_length, char *match, int match_len, int trailing);

// dir.c
char *list_directory(char *directory, int len, int *n_entries);
char *get_home_directory(void);
bool find_program(char *name, char **error_str);
int stat_ex(char *str, int len, struct stat *s);
bool is_dir(char *str, int len);
bool is_file(char *str, int len);

// font.c
char *get_font_path(char *name);
int glyph_indexof(char c);
bool open_font(char *font_path);
bool render_font(Screen_Info *info, float font_size, bool italics, u32 foreground, u32 background, Glyph *chars);
void close_font();

// gui.c
bool open_display(int screen_idx, Screen_Info *screen_info);
void close_display();
int run_gui(GUI_Settings *settings, Screen_Info *screen_info, Font_Renders *renders, char *textbox, int textbox_len, char *error_msg);

#endif
