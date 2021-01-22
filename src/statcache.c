#include <string.h> /* memset */
#include <stdlib.h> /* malloc */
#include <stdio.h> /* printf */
#include <errno.h>

#include "statcache.h"
#include "support.h"
#include "mem.h"
#include "path.h"

#define STATCACHE_HASH_SIZE (16*1024)

struct STATCACHE_ENTRY
{
	hash_t hashid;
	time_t timestamp;
	unsigned int isregular:1;
	struct STATCACHE_ENTRY* next;
};

struct STATCACHE
{
	struct HEAP* heap;
	struct STATCACHE_ENTRY* entries[STATCACHE_HASH_SIZE];
};

struct STATCACHE* statcache_create()
{
	struct STATCACHE* statcache = malloc(sizeof(struct STATCACHE));
	memset(statcache, 0, sizeof(struct STATCACHE));

	statcache->heap = mem_create();
	return statcache;
}

void statcache_free(struct STATCACHE* statcache)
{
	if(!statcache)
		return;
	mem_destroy(statcache->heap);
	free( statcache );
}

struct STATCACHE_ENTRY* statcache_getstat_int(struct STATCACHE* statcache, const char* filename)
{
	hash_t namehash = string_hash( filename );
	int hashindex = namehash & ( STATCACHE_HASH_SIZE - 1 );

	struct STATCACHE_ENTRY* entry = NULL;
	for ( entry = statcache->entries[ hashindex ]; entry; entry = entry->next ) {
		if ( entry->hashid == namehash ) {
			break;
		}
	}

	if ( !entry ) {
		entry = ( struct STATCACHE_ENTRY* )mem_allocate( statcache->heap, sizeof( struct STATCACHE_ENTRY ) );
		entry->hashid = namehash;
		entry->timestamp = file_timestamp( filename );
		if ( entry->timestamp != 0 ) {
			entry->isregular = file_isregular( filename );
		}
		entry->next = statcache->entries[ hashindex ];
		statcache->entries[ hashindex ] = entry;
	}
	return entry;
}

int statcache_getstat(struct STATCACHE* statcache, const char* filename, time_t* timestamp, int* isregularfile)
{
	if(!statcache)
		return 1;
	
	// check if the folder exists first. Many files share the same folder path,
	// so this check is likely to hit the cache even if the full path is unique
	// total number stats is 38% of what it was without in my (large, actual project) test
	char dirname[MAX_PATH_LENGTH];
	if(path_directory(filename, dirname, MAX_PATH_LENGTH) == 0 && dirname[0] != '\0')
	{
		struct STATCACHE_ENTRY* direntry = statcache_getstat_int(statcache, dirname);
		if(direntry)
		{
			if(direntry->timestamp == 0)
			{
				*timestamp = 0;
				*isregularfile = 0;
				return 0;
			}
		}
	}
	
	struct STATCACHE_ENTRY* entry = statcache_getstat_int(statcache, filename);
	
	*timestamp=entry->timestamp;
	*isregularfile = entry->isregular;
	return 0;
}
