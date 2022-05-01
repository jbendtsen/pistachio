#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <pwd.h>
#include <unistd.h>

#include "pistachio.h"

#define POOL_SIZE 1024 * 1024

Listing *listings = NULL;
Listing **list_head = NULL;

static Arena arena = {0};

void init_directory_arena() {
	make_arena(POOL_SIZE, &arena);
	list_head = &listings;
}

Listing *current = NULL;

static int compare_entries(const void *p1, const void *p2) {
	Listing *l = current;
	int idx1 = *(int*)p1;
	int idx2 = *(int*)p2;

	int mode1 = l->stats[idx1].st_mode & S_IFDIR;
	int mode2 = l->stats[idx2].st_mode & S_IFDIR;

	if (mode1 && !mode2)
		return -1;
	if (!mode1 && mode2)
		return 1;

	return strcmp(l->table[idx1], l->table[idx2]);
}

void sort_entries(Listing *l, char *path) {
	if (arena.idx % sizeof(char*))
		allocate(&arena, sizeof(char*) - (arena.idx % sizeof(char*)));

	l->index = (int*)allocate(&arena, l->n_entries * sizeof(int));
	l->index[0] = 0;

	l->table = (char**)allocate(&arena, l->n_entries * sizeof(char*));
	l->table[0] = l->first;

	int path_len = strlen(path);
	strcpy(&path[path_len++], "/");

	l->stats = (struct stat*)allocate(&arena, l->n_entries * sizeof(struct stat));
	strcpy(&path[path_len], l->table[0]);
	stat(path, &l->stats[0]);

	for (int i = 1; i < l->n_entries; i++) {
		l->index[i] = i;

		int sz = strlen(l->table[i-1]) + 1;
		l->table[i] = &l->table[i-1][sz];

		strcpy(&path[path_len], l->table[i]);
		if (lstat(path, &l->stats[i]) != 0)
			memset(&l->stats[i], 0, sizeof(struct stat));
	}

	current = l;
	qsort(l->index, l->n_entries, sizeof(int), compare_entries);
}

void get_directory_entries(DIR *d, Listing *l) {
	// This codebase depends on directory entries being laid out contiguously in memory,
	//  so we temporarily disable the ability for this arena to spill over to another pool.
	arena.allow_overflow = false;

	char *prev = NULL;
	struct dirent *ent;
	while ((ent = readdir(d))) {
		if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
			continue;

		int ent_sz = strlen(ent->d_name) + 1;
		char *str = allocate(&arena, ent_sz);

		if (!str) {
			// If there was enough room for the string
			if (arena.idx <= arena.pool_size - ent_sz)
				break;

			int allocated = prev == NULL ? 0 : (int)(prev - l->first) + strlen(prev) + 1;
			int size = allocated + ent_sz;
			// If this set of entries takes up more room than a single pool
			if (size > arena.pool_size)
				break;

			// Copy this set of entries over to the next pool
			find_next_pool(&arena);
			if (l->first && prev) {
				char *start = l->first;
				l->first = allocate(&arena, size);
				memcpy(l->first, start, allocated);
				str = &l->first[allocated];
			}
			else
				str = allocate(&arena, ent_sz);
		}

		strcpy(str, ent->d_name);

		l->n_entries++;
		if (!l->first)
			l->first = str;

		prev = str;
	}

	arena.allow_overflow = true;
}

bool list_directory(char *directory, int len, Listing *info) {
	if (len < 0)
		len = strlen(directory);

	if (listings) {
		Listing *l = listings;
		while (l) {
			if (l->name && !strncmp(directory, l->name, len)) {
				memcpy(info, l, sizeof(Listing));
				return true;
			}
			l = l->next;
		}
	}

	DIR *d = NULL;
	char path[4096];

	if (directory[0] == '~') {
		char *home = get_home_directory();
		int home_len = strlen(home);

		strcpy(path, home);
		strncpy(&path[home_len], &directory[1], len);
		path[home_len + len] = 0;

		d = opendir(path);
	}
	else {
		strncpy(path, directory, len);
		path[len] = 0;
	}

	d = opendir(path);
	if (!d) {
		memset(info, 0, sizeof(Listing));
		return false;
	}

	Listing *l = (Listing*)allocate(&arena, sizeof(Listing));
	*list_head = l;
	list_head = &l->next;

	memset(l, 0, sizeof(Listing));
	l->name = allocate(&arena, len + 1);
	memcpy(l->name, directory, len + 1);

	get_directory_entries(d, l);

	closedir(d);

	if (l->n_entries > 0)
		sort_entries(l, path);

	memcpy(info, l, sizeof(Listing));
	return true;
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
		home_dir = allocate(&arena, len + 1);
		strcpy(home_dir, home);
	}

	return home_dir;
}

char *get_desugared_path(char *str, int len) {
	char *home = NULL;
	int home_len = 0;
	int offset = 0;

	if (str[0] == '~') {
		home = get_home_directory();
		home_len = strlen(home);
		offset = 1;
	}

	len -= offset;

	char *path = allocate(&arena, len + home_len + 1);

	if (home) strcpy(path, home);
	memcpy(&path[home_len], &str[offset], len);
	path[home_len + len] = 0;
	remove_backslashes(path, -1);

	return path;
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
		Listing list;
		list_directory(path, len, &list);

		bool found = false;
		for (int i = 0; i < list.n_entries; i++) {
			if (!strcmp(list.table[i], name)) {
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
