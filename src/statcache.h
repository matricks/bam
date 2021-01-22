#include "support.h"

struct STATCACHE;

/*
	Runtime file stat cache
	So we don't have to stat a file more than once per run.
*/

struct STATCACHE* statcache_create();
void statcache_free(struct STATCACHE* statcache);
int statcache_getstat(struct STATCACHE* statcache, const char* filename, time_t* timestamp, int* isregularfile);

