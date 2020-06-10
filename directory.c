#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

#include "pistachio.h"

#define POOL_SIZE 1024 * 1024

struct _Listing {
	char *name;
	struct _Listing *next;
	char *first;
	int n_entries;
};

typedef struct _Listing Listing;

Listing *listings = NULL;
Listing **list_head = NULL;

static char *pool = NULL;
static int head = 0;

static char *allocate(int size) {
	if (head + size > POOL_SIZE)
		return NULL;

	char *ptr = &pool[head];
	head += size;
	return ptr;
}

static void destroy_pool() {
	free(pool);
}

char *list_directory(char *directory, int len, int *n_entries) {
	if (len < 0)
		len = strlen(directory);

	if (!pool) {
		pool = malloc(POOL_SIZE);
		atexit(destroy_pool);
		list_head = &listings;
	}

	if (listings) {
		Listing *l = listings;
		while (l) {
			if (!strcmp(directory, l->name)) {
				*n_entries = l->n_entries;
				return l->first;
			}
			l = l->next;
		}
	}

	DIR *d = NULL;
	if (directory[0] == '~') {
		char *home = get_home_directory();
		int home_len = strlen(home);
		char path[home_len + len + 1];

		strcpy(path, home);
		strncpy(&path[home_len], &directory[1], len);
		path[home_len + len] = 0;

		d = opendir(path);
	}
	else {
		char path[len + 1];
		strncpy(path, directory, len);
		path[len] = 0;

		d = opendir(path);
	}

	if (!d)
		return NULL;

	Listing *l = (Listing*)allocate(sizeof(Listing));
	*list_head = l;
	list_head = &l->next;

	memset(l, 0, sizeof(Listing));
	l->name = allocate(len + 1);
	memcpy(l->name, directory, len + 1);

	struct dirent *ent;
	while ((ent = readdir(d))) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		char *str = allocate(strlen(ent->d_name) + 1);
		if (!str)
			break;
		else
			strcpy(str, ent->d_name);

		l->n_entries++;
		if (!l->first)
			l->first = str;
	}

	closedir(d);

	*n_entries = l->n_entries;
	return l->first;
}

int stat_ex(char *str, int len, struct stat *s) {
	char *home = NULL;
	int home_len = 0;
	int offset = 0;

	if (str[0] == '~') {
		home = get_home_directory();
		home_len = strlen(home);
		offset = 1;
	}

	char path[len + home_len + 1];
	if (home)
		strcpy(path, home);

	strcpy(&path[home_len], &str[offset]);
	remove_backslashes(path, -1);

	return stat(path, s);
}

bool is_dir(char *str, int len) {
	struct stat s;
	if (stat_ex(str, len, &s) != 0)
		return false;

	return (s.st_mode & S_IFMT) == S_IFDIR;
}

bool is_file(char *str, int len) {
	struct stat s;
	if (stat_ex(str, len, &s) != 0)
		return false;

	return (s.st_mode & S_IFMT) == S_IFREG;
}

char *home_dir = NULL;

char *get_home_directory() {
	if (home_dir)
		return home_dir;

	char *home = getenv("HOME");
	if (!home) {
		struct passwd *ps = getpwuid(getuid());
		if (ps)
			home = ps->pw_dir;
	}

	if (home) {
		int len = strlen(home);
		home_dir = allocate(len + 1);
		strcpy(home_dir, home);
	}

	return home_dir;
}

bool find_program(char *name, char **error_str) {
	char *bin_path = getenv("PATH");
	char *next = bin_path;
	char file[200];

	char *p = &bin_path[-1];
	do {
		p++;
		if (*p != ':' && *p != 0)
			continue;

		char *path = next;
		next = p + 1;

		int len = p - path;
		int n_results = 0;
		char *results = list_directory(path, len, &n_results);

		bool found = false;
		char *str = results;
		for (int i = 0; i < n_results; i++, str += strlen(str) + 1) {
			if (!strcmp(str, name)) {
				found = true;
				break;
			}
		}
		if (!found) {
			if (error_str) *error_str = "command not found: ";
			continue;
		}

		if (error_str) *error_str = NULL;

		sprintf(file, "%.*s/%s", len, path, name);

		struct stat s;
		if (stat(file, &s) != 0) {
			if (error_str) *error_str = "command not found: ";
			break;
		}

		if ((s.st_mode & S_IFMT) == S_IFDIR) {
			if (error_str) *error_str = "not a regular file: ";
			break;
		}

		if ((s.st_mode & S_IXUSR) == 0) {
			if (error_str) *error_str = "missing execute permission: ";
			break;
		}

		return true;

	} while (*p);

	return false;
}
