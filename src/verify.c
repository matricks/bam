#include <stdio.h>
#include <string.h>
#include "support.h"
#include "mem.h"
#include "verify.h"

struct VERIFY_FILE {
	hash_t hashid;
	time_t timestamp;

	unsigned lastseen; /* which run we last saw this node */

	struct VERIFY_FILE *hashnext; /* next file in the hash chain */
	struct VERIFY_FILE *next; /* next file in the global list */
	char path[];
};

struct VERIFY_STATE {
	struct HEAP * heap;
	const char *basepath;

	/* file database */
	struct VERIFY_FILE *firstfile;
	struct VERIFY_FILE *files[0x100000];

	unsigned updatecount; /* number of updates done */

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
				state->errors += state->callback(fullpath, hashid, node->timestamp, timestamp, state->user);
				node->timestamp = timestamp;
				node->lastseen = state->updatecount;
				break;
			}

			node = node->hashnext;
		}

		/* no node found? then add it */
		if(node == NULL)
		{
			node = mem_allocate(state->heap, sizeof(struct VERIFY_FILE) + path_len + 1);
			node->hashid = hashid;
			node->timestamp = timestamp;
			node->next = state->firstfile;
			node->hashnext = state->files[hashid&0xfffff];
			node->lastseen = state->updatecount;
			state->firstfile = node;
			state->files[hashid&0xfffff] = node;
			memcpy(node->path, fullpath, path_len + 1);

			state->errors += state->callback(fullpath, hashid, 0, timestamp, state->user);
		}
	}
}

struct VERIFY_STATE *verify_create(const char *basepath)
{
	struct HEAP *heap = mem_create();
	struct VERIFY_STATE *state = mem_allocate(heap, sizeof(struct VERIFY_STATE));
	state->heap = heap;
	state->basepath = basepath;
	return state;
}

void verify_destroy(struct VERIFY_STATE *state)
{
	mem_destroy(state->heap);
}

int verify_update(struct VERIFY_STATE *state, int (*callback)(const char *fullpath, hash_t hashid, time_t oldstamp, time_t newstamp, void *user), void *user)
{
	struct VERIFY_FILE *node;

	/* setup for run */
	state->callback = callback;
	state->user = user;
	state->errors = 0;
	state->updatecount++;

	/* scan the filesystem for changes */
	file_listdirectory(state->basepath, list_callback, state);

	/* check for deleted files */
	for(node = state->firstfile; node != NULL; node = node->next)
	{
		if(node->lastseen < state->updatecount && node->timestamp != 0)
		{
			state->errors += callback(node->path, node->hashid, node->timestamp, 0, state->user);
			node->timestamp = 0;
		}
	}

	return state->errors;
}


