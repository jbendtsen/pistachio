#ifndef PISTACHIO_H
#define PISTACHIO_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define FONT_WIDTH(glyphs) (int)(glyphs[0].box_w + 0.5)
#define FONT_HEIGHT(glyphs) (int)(glyphs[0].box_h + 0.5)

#define LERP_ARGB(fore, back, lum) \
	((u32)(fore->a + (back->a - fore->a) * lum) << 24) | \
	((u32)(fore->r + (back->r - fore->r) * lum) << 16) | \
	((u32)(fore->g + (back->g - fore->g) * lum) << 8) | \
	(u32)(fore->b + (back->b - fore->b) * lum)

#define MIN_CHAR ' '
#define MAX_CHAR '~'
#define N_CHARS  (MAX_CHAR - MIN_CHAR + 1)

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
	int search_font_size;
	int results_font_size;
	u32 foreground;
	u32 background;
	u32 caret;
	u32 selected;
	ARGB fore_argb;
	ARGB back_argb;
	ARGB caret_argb;
	ARGB sel_argb;
} GUI_Settings;

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
} Font_Renders;

// utils.c
void make_argb(u32 color, ARGB *argb);
void remove_char(char *str, int len, int pos);
int insert_chars(char *str, int len, char *insert, int insert_len, int pos);
int insert_substring(char *str, int len, char *insert, int insert_len, int pos);
bool enumerate_directory(char *textbox, int cursor, char **word, int *word_length, int *search_length, char **result, int *n_entries);
int auto_complete(char *word, int *word_length, int max_len, char *results, int n_results, int trailing);

// dir.c
char *list_directory(char *directory, int *n_entries);
char *get_home_directory(void);
bool is_dir(char *str, int len);
bool is_file(char *str, int len);

// font.c
char *get_font_path(char *name);
int glyph_indexof(char c);
bool open_font(char *font_path);
bool render_font(Screen_Info *info, int font_size, ARGB *fore, ARGB *back, Glyph *chars);
void close_font();

// gui.c
bool open_display(int screen_idx, Screen_Info *screen_info);
void close_display();
int run_gui(GUI_Settings *settings, Screen_Info *screen_info, Font_Renders *renders, char *textbox, int textbox_len);

#endif
