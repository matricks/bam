#include "support.h"

struct GRAPH;
struct DEPCACHE;
struct SCANCACHE;
struct OUTPUTCACHE; /* bad name */
struct CONTEXT;
struct NODE;

/*
	Dependecy cache
	The dependecy cache keeps a list of dependencies for every node.
*/
int depcache_save(const char *filename, struct GRAPH *graph);
struct DEPCACHE *depcache_load(const char *filename);
void depcache_free(struct DEPCACHE *depcache);
struct CACHEINFO_DEPS *depcache_find_byhash(struct DEPCACHE *cache, hash_t hashid);
struct CACHEINFO_DEPS *depcache_find_byindex(struct DEPCACHE *cache, unsigned index);
int depcache_do_dependency(
	struct CONTEXT *context,
	struct NODE *node,
	void (*callback)(struct NODE *node, struct CACHEINFO_DEPS *cacheinfo, void *user),
	void *user);

/*
	Scan cache
	Cache for C source files and what headers they reference in them
*/
int scancache_save(const char *filename, struct GRAPH *graph);
struct SCANCACHE *scancache_load(const char *filename);
struct CHEADERREF *scancache_find(struct SCANCACHE *scancache, struct NODE * node);
void scancache_free(struct SCANCACHE *scancache);

/*
	Output cache
	Keeps the latest commandline and timestamp that was used to build that output.
*/

int outputcache_save(const char *filename, struct OUTPUTCACHE *oldcache, struct GRAPH *graph, time_t cache_timestamp);
struct OUTPUTCACHE *outputcache_load(const char *filename, time_t *cache_timestamp);
void outputcache_free(struct OUTPUTCACHE *outputcache);
struct CACHEINFO_OUTPUT *outputcache_find_byhash(struct OUTPUTCACHE *outputcache, hash_t hashid);
