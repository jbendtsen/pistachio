#include "pistachio.h"

void make_argb(u32 color, ARGB *argb) {
	argb->a = (float)(0xff & (color >> 24));
	argb->r = (float)(0xff & (color >> 16));
	argb->g = (float)(0xff & (color >> 8));
	argb->b = (float)(0xff & color);
}

void remove_char(char *str, int len, int pos) {
	if (pos > 0 && pos <= len) {
		memmove(&str[pos-1], &str[pos], len - pos);
		str[len-1] = 0;
	}
}

int insert_substring(char *str, int len, char *insert, int insert_len, int pos) {
	if (pos < 0 || pos > len || insert_len <= 0)
		return 0;

	if (pos == len)
		strncpy(&str[pos], insert, insert_len);
	else {
		memmove(&str[pos + insert_len], &str[pos], len - pos);
		memcpy(&str[pos], insert, insert_len);
	}

	return insert_len;
}

int insert_chars(char *str, int len, char *insert, int insert_len, int pos) {
	int add_len = 0;
	char add[insert_len];
	for (int i = 0; i < insert_len; i++) {
		if (glyph_indexof(insert[i]) >= 0)
			add[add_len++] = insert[i];
	}

	return insert_substring(str, len, add, add_len, pos);
}

bool enumerate_directory(char *textbox, int cursor, char **word, int *word_length, int *search_length, char **result, int *n_entries) {
	int input_len = strlen(textbox);
	if (!input_len)
		return textbox;

	int start = 0;
	for (int i = 0; i < cursor && i < input_len; i++) {
		if (textbox[i] == ' ')
			start = i+1;
	}

	int end;
	for (end = start; textbox[end] != ' ' && textbox[end]; end++);
	end--;

	int word_len = end - start + 1;
	int d_len = word_len > 10 ? word_len : 10;
	char directory[d_len];
	memset(directory, 0, d_len);

	int search_len = 0;
	bool is_command = !(textbox[start] == '/' || textbox[start] == '~');
	if (!is_command) {
		for (int i = end; i >= start && (textbox[i] != '/' && textbox[i] != '~'); i--)
			search_len++;

		int extra = 0;
		if (word_len - search_len > 1) extra = 1;

		memcpy(directory, &textbox[start], word_len - search_len - extra);
	}
	else {
		strcpy(directory, "/usr/bin");
		search_len = word_len;
	}

	*result = list_directory(directory, n_entries);
	if (word)
		*word = &textbox[start];
	if (word_length)
		*word_length = word_len;
	if (search_length)
		*search_length = search_len;

	return is_command;
}

int auto_complete(char *word, int *word_length, int max_len, char *results, int n_results, int trailing) {
	int word_len = *word_length;

	char *first = NULL;
	int match_len = 0;

	if (trailing) {
		char *str = results;
		for (int i = 0; i < n_results; i++, str += strlen(str) + 1) {
			if (strncmp(str, word + word_len - trailing, trailing))
				continue;

			if (!first) {
				first = str;
				match_len = strlen(first);
				continue;
			}

			int j;
			for (j = trailing; j < match_len && str[j] == first[j]; j++);
			match_len = j;
		}
	}
	else if (n_results == 1) {
		first = results;
		match_len = strlen(first);
	}

	int add = match_len - trailing;

	if (first)
		word_len += insert_substring(word, strlen(word), &first[trailing], add, word_len);

	if (is_dir(word, word_len)) {
		trailing = 0;
		if (word[word_len-1] != '/')
			insert_substring(word, strlen(word), "/", 1, word_len);
	}
	else if (first)
		trailing = match_len;

	if (word_length)
		*word_length = word_len;

	return trailing;
}
