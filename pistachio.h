#ifndef PISTACHIO_H
#define PISTACHIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>

#define FONT_WIDTH(glyph) (int)(glyph.box_w + 0.5)
#define FONT_HEIGHT(glyph) (int)(glyph.box_h + 0.5)

#define MIN_CHAR ' '
#define MAX_CHAR '~'
#define N_CHARS  (MAX_CHAR - MIN_CHAR + 1)

#define RES_OFFSET 0
#define SEL_OFFSET (4 * N_CHARS)
#define BAR_OFFSET (8 * N_CHARS)
#define ERR_OFFSET (9 * N_CHARS)
#define N_RENDERS (10 * N_CHARS)

#define BINARIES_DIR  "/usr/bin"

#define SIZEOF_XIMAGE 136

#define STATUS_EXIT     0
#define STATUS_COMMAND  1

typedef unsigned char u8;
typedef unsigned int u32;

typedef struct {
	int pool_size;
	int pool;
	int idx;
	bool allow_overflow;
	bool initialized;
} Arena;

struct listing_struct {
	char *name;
	struct listing_struct *next;
	char *first;
	int *index;
	char **table;
	struct stat *stats;
	int n_entries;
};
typedef struct listing_struct Listing;

struct program_struct {
	char *command;
	char *extensions;
	int n_extensions;
	bool daemonize;
	struct program_struct *next;
};
typedef struct program_struct Program;

typedef struct {
	float size;
	u32 color;
	bool oblique;
	bool bold;
} Font_Attrs;

typedef struct {
	float window_w;
	float window_h;
	char *font_path;
	Font_Attrs search_font;
	Font_Attrs results_font;
	Font_Attrs error_font;
	u32 back_color;
	u32 caret_color;
	u32 selected_color;
	Program terminal_program;
	Program folder_program;
	Program default_program;
	Program *programs;
} Settings;

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
	char ximage[SIZEOF_XIMAGE];
} Glyph;

typedef struct {
	float a, r, g, b;
} ARGB;

// arena.c
void make_arena(int pool_size, Arena *a);
void find_next_pool(Arena *a);
void *allocate(Arena *a, int size);
void defer_arena_destruction(void);

// config.c
Settings *load_config(void);
void save_config(char *path);

// directory.c
void init_directory_arena(void);
bool list_directory(char *directory, int len, Listing *info);
char *get_home_directory(void);
char *get_desugared_path(char *str, int len);
bool find_program(char *name, char **error_str);

// font.c
int glyph_indexof(char c);
bool open_font(char *font_path);
bool render_font(Screen_Info *info, Font_Attrs *attrs, u32 background, Glyph *chars);
void close_font(void);

// gui.c
bool open_display(int screen_idx, Screen_Info *screen_info);
void close_display(void);
int run_gui(Settings *config, Screen_Info *screen_info, Glyph *renders, char *textbox, int textbox_len, char *error_msg);

// utils.c
void make_argb(u32 color, ARGB *argb);
void remove_char(char *str, int len, int pos);
int insert_chars(char *str, int len, char *insert, int insert_len, int pos);
int insert_substring(char *str, int len, char *insert, int insert_len, int pos);
int remove_backslashes(char *str, int span);
int escape_spaces(char *str, int span);
int find_next_word(char *str, int start, int end);
void prepend_word(char *word, char *sentence);
bool difference_ignoring_backslashes(char *str, char *word, int word_len, int trailing);
bool enumerate_directory(char *textbox, int cursor, char **word, int *word_length, int *search_length, Listing *list);
char *find_completeable_span(Listing *listing, char *word, int word_len, int trailing, int *match_length);
int complete(char *word, int *word_length, char *match, int match_len, int trailing, bool folder_completion);

#endif
