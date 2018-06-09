#include <stdio.h>
#include <string.h>
#include "support.h"
#include "mem.h"
#include "verify.h"

struct VERIFY_FILE {
	hash_t hashid;
	time_t timestamp;
	struct VERIFY_FILE *next;
	char path[];
};

struct VERIFY_STATE {
	struct HEAP * heap;

	/* file database */
	struct VERIFY_FILE *files[0x100000];

	int errors;

	/* callback */
	int (*callback)(const char *fullpath, hash_t hashid, time_t oldstamp, time_t newstamp, void *user);
	void *user;
};

static void list_callback(const char *fullpath, const char *filename, int dir, void *user)
{
	/* gather information */
	struct VERIFY_STATE *state = user;
	hash_t hashid = string_hash(fullpath);
	int path_len = strlen(fullpath);
	time_t timestamp = file_timestamp(fullpath);

	/* ignore hidden files and directories */
	if(filename[0] == '.')
		return;

	if(dir)
	{
		/* recurse into directory */
		file_listdirectory(fullpath, list_callback, user);
	}
	else
	{
		/* search for the file */
		struct VERIFY_FILE *node = state->files[hashid&0xfffff];
		while(node != NULL)
		{
			if(node->hashid == hashid)
			{
				/* file found */
				if(state->callback(fullpath, hashid, node->timestamp, timestamp, state->user))
					state->errors++;
				node->timestamp = timestamp;
				break;
			}

			node = node->next;
		}

		/* no node found? then add it */
		if(node == NULL)
		{
			node = mem_allocate(state->heap, sizeof(struct VERIFY_FILE) + path_len + 1);
			node->hashid = hashid;
			node->timestamp = timestamp;
			node->next = state->files[hashid&0xfffff];
			state->files[hashid&0xfffff] = node;
			memcpy(node->path, fullpath, path_len + 1);

			if(state->callback(fullpath, hashid, 0, timestamp, state->user))
				state->errors++;
		}
	}
}

struct VERIFY_STATE *verify_create()
{
	struct HEAP *heap = mem_create();
	struct VERIFY_STATE *state = mem_allocate(heap, sizeof(struct VERIFY_STATE));
	state->heap = heap;
	return state;
}

void verify_destroy(struct VERIFY_STATE *state)
{
	mem_destroy(state->heap);
}

int verify_update(struct VERIFY_STATE *state, int (*callback)(const char *fullpath, hash_t hashid, time_t oldstamp, time_t newstamp, void *user), void *user)
{
	state->callback = callback;
	state->user = user;
	state->errors = 0;
	file_listdirectory("", list_callback, state);
	return state->errors;
}


