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
		memcpy(&str[pos], insert, insert_len);
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

int find_next_word(char *str, int start, int end) {
	bool was_backslash = false;
	for (int i = start; i < end; i++) {
		if (str[i] == ' ' && !was_backslash)
			return i+1;

		was_backslash = str[i] == '\\';
	}

	return start;
}

void find_word(char *str, int idx, int *first, int *last) {
	int len = strlen(str);
	if (idx >= len)
		idx = len-1;

	bool seen_non_space = false;
	int i = idx;
	while (true) {
		if (i == 0) {
			*first = str[0] == ' ' ? 1 : 0;
			break;
		}
		if (str[i] == ' ' && str[i-1] != '\\' && seen_non_space) {
			*first = i+1;
			break;
		}
		if (!seen_non_space)
			seen_non_space = str[i] != ' ';
		i--;
	}

	bool was_backslash = false;
	i = *first;
	while (true) {
		if (str[i] == ' ' && !was_backslash) {
			*last = i-1;
			break;
		}
		if (i == len-1) {
			*last = i;
			break;
		}
		was_backslash = str[i++] == '\\';
	}
}

void prepend_word(char *word, char *sentence) {
	if (!word || !sentence)
		return;

	int insert_len = strlen(word) + 1;
	if (insert_len <= 1)
		return;

	memmove(&sentence[insert_len], sentence, strlen(sentence) + 1);
	strcpy(sentence, word);
	sentence[insert_len-1] = ' ';
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
	if (!input_len || textbox[input_len-1] == ' ')
		return true;

	int first = 0, last = 0;
	find_word(textbox, cursor, &first, &last);

	int word_len = last - first + 1;
	int d_len = word_len > 10 ? word_len : 10;
	char directory[d_len];
	memset(directory, 0, d_len);

	int search_len = 0;
	bool is_command = !(textbox[first] == '/' || textbox[first] == '~');
	if (!is_command) {
		for (int i = last; i >= first && (textbox[i] != '/' && textbox[i] != '~'); i--)
			search_len++;

		int extra = 0;
		if (word_len - search_len > 1) extra = 1;

		d_len = word_len - search_len - extra;
		memcpy(directory, &textbox[first], d_len);
		directory[d_len] = 0;

		remove_backslashes(directory, -1);
	}
	else {
		strcpy(directory, BINARIES_DIR);
		search_len = word_len;
	}

	*result = list_directory(directory, -1, n_entries);
	if (word)
		*word = &textbox[first];
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

int complete(char *word, int *word_length, char *match, int match_len, int trailing, bool folder_completion) {
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

	struct stat s;
	char *path = get_desugared_path(word, word_len);

	// If folder completion is enabled and 'word' refers to a folder, append a forward slash for further tab completion
	if (folder_completion && stat(path, &s) == 0 && (s.st_mode & S_IFMT) == S_IFDIR) {
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
