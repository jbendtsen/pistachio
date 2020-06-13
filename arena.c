// A non-thread-safe expandable table of memory pools

#include "pistachio.h"

#define POOLS_PER_STAND 16

char **pools = NULL;
int n_pools = 0;
int table_len = 0;

void find_next_pool(Arena *a) {
	while (a->pool < table_len && pools[a->pool])
		a->pool++;

	if (a->pool >= table_len) {
		pools = realloc(pools, (table_len + POOLS_PER_STAND) * sizeof(char*));
		memset(&pools[table_len], 0, POOLS_PER_STAND * sizeof(char*));
		table_len += POOLS_PER_STAND;
	}

	if (a->pool >= n_pools)
		n_pools = a->pool + 1;
}

void make_arena(int pool_size, Arena *a) {
	if (!pools) {
		pools = calloc(POOLS_PER_STAND, sizeof(char*));
		table_len = POOLS_PER_STAND;
	}

	*a = (Arena) {
		.pool_size = pool_size,
		.pool = 0,
		.idx = 0,
		.allow_overflow = true,
		.initialized = true
	};

	find_next_pool(a);
	pools[a->pool] = malloc(a->pool_size);
}

void *allocate(Arena *a, int size) {
	if (a->idx + size > a->pool_size) {
		if (!a->allow_overflow)
			return NULL;

		a->idx = 0;
		find_next_pool(a);

		// if the allocation request is too large for a pool, make it its own pool
		if (size > a->pool_size) {
			pools[a->pool] = malloc(size);
			void *ptr = (void*)pools[a->pool];

			find_next_pool(a);
			return ptr;
		}

		if (!pools[a->pool])
			pools[a->pool] = malloc(a->pool_size);
	}

	void *ptr = (void*)&pools[a->pool][a->idx];
	a->idx += size;
	return ptr;
}

void destroy_all_arenas() {
	if (!pools)
		return;

	for (int i = 0; i < table_len; i++) {
		if (pools[i])
			free(pools[i]);
	}

	free(pools);
	pools = NULL;
}

void defer_arena_destruction() {
	atexit(destroy_all_arenas);
}
