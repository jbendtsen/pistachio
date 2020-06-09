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
	if (len < 0)
		len = strlen(str);

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

int remove_backslashes(char *str, int span) {
	int len = strlen(str);
	if (span < 0)
		span = len;

	int idx = 0;
	for (int i = 0; i < span; i++) {
		if (str[i] == '\\')
			continue;

		str[idx++] = str[i];
	}

	if (len > span)
		memmove(&str[idx], &str[span], len - span + 1); // +1 for null terminator
	else
		str[idx] = 0;

	return idx;
}

int escape_spaces(char *str, int span) {
	int len = strlen(str);
	if (span < 0)
		span = len;

	bool was_backslash = false;
	for (int i = 0; i < span; i++) {
		if (str[i] == ' ' && !was_backslash) {
			memmove(&str[i+1], &str[i], len - i);
			str[i++] = '\\';
			span++;
			len++;
		}
		was_backslash = str[i] == '\\';
	}

	return span;
}

int find_word(char *str, int start, int end) {
	bool was_backslash = false;
	for (int i = start; i < end; i++) {
		if (str[i] == ' ' && !was_backslash)
			return i+1;

		was_backslash = str[i] == '\\';
	}

	return start;
}

bool difference_ignoring_backslashes(char *name, char *word, int word_len, int trailing) {
	if (!trailing)
		return false;

	char *search = &word[word_len - trailing];
	for (int i = 0; i < trailing && search[i]; i++) {
		if (name[i] == '\\')
			name++;
		if (search[i] == '\\')
			search++;

		if (name[i] != search[i])
			return true;
	}

	return false;
}

bool enumerate_directory(char *textbox, int cursor, char **word, int *word_length, int *search_length, char **result, int *n_entries) {
	int input_len = strlen(textbox);
	if (!input_len)
		return textbox;

	int start = find_word(textbox, 0, cursor);
	int end = find_word(textbox, start, input_len);
	if (end == start)
		end = input_len;

	int word_len = end - start;
	int d_len = word_len > 10 ? word_len : 10;
	char directory[d_len];
	memset(directory, 0, d_len);

	int search_len = 0;
	bool is_command = !(textbox[start] == '/' || textbox[start] == '~');
	if (!is_command) {
		for (int i = end-1; i >= start && (textbox[i] != '/' && textbox[i] != '~'); i--)
			search_len++;

		int extra = 0;
		if (word_len - search_len > 1) extra = 1;

		d_len = word_len - search_len - extra;
		memcpy(directory, &textbox[start], d_len);
		directory[d_len] = 0;

		remove_backslashes(directory, -1);
	}
	else {
		strcpy(directory, BINARIES_DIR);
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

char *find_completeable_span(char *word, int word_len, char *results, int n_results, int trailing, int *match_length) {
	char *match = NULL;
	int match_len = 0;

	if (trailing) {
		char *str = results;
		for (int i = 0; i < n_results; i++, str += strlen(str) + 1) {
			if (difference_ignoring_backslashes(str, word, word_len, trailing))
				continue;

			if (!match) {
				match = str;
				match_len = strlen(match);
				continue;
			}

			int j;
			for (j = trailing; j < match_len && str[j] == match[j]; j++);
			match_len = j;
		}
	}
	else if (n_results == 1) {
		match = results;
		match_len = strlen(match);
	}

	if (match_length)
		*match_length = match_len;

	return match;
}

int complete(char *word, int *word_length, char *match, int match_len, int trailing) {
	int word_len = *word_length;

	int n_spaces = 0;
	for (int i = 0; i < trailing; i++) {
		if (match[i] == ' ')
			n_spaces++;
	}

	int offset = trailing - n_spaces;
	int add = match_len - offset;

	if (word_len > 0 && word[0] == '~' && (word_len == 1 || word[1] != '/'))
		word_len += insert_substring(word, -1, "/", 1, 1);

	word_len += insert_substring(word, -1, &match[offset], add, word_len);
	word_len = escape_spaces(word, word_len);

	if (is_dir(word, word_len)) {
		trailing = 0;
		if (word[word_len-1] != '/')
			word_len += insert_substring(word, -1, "/", 1, word_len);
	}
	else if (match)
		trailing = match_len;

	if (word_length)
		*word_length = word_len;

	return trailing;
}
