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

char *list_directory(char *directory, int *n_entries) {
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
		int len = strlen(home) + strlen(directory);
		char path[len+1];
		strcpy(path, home);
		strcpy(path + strlen(home), directory + 1);
		d = opendir(path);
	}
	else
		d = opendir(directory);

	if (!d)
		return NULL;

	Listing *l = (Listing*)allocate(sizeof(Listing));
	*list_head = l;
	list_head = &l->next;

	memset(l, 0, sizeof(Listing));
	l->name = allocate(strlen(directory) + 1);
	memcpy(l->name, directory, strlen(directory) + 1);

	struct dirent *ent;
	while (ent = readdir(d)) {
		int len = strlen(ent->d_name);
		char *str = allocate(len + 1);
		if (!str)
			break;
		else
			memcpy(str, ent->d_name, len + 1);

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
