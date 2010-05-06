struct GRAPH;
struct CACHE;
struct CONTEXT;
struct NODE;

/* cache */
int cache_save(const char *filename, struct GRAPH *graph);
struct CACHE *cache_load(const char *filename);
struct CACHENODE *cache_find_byhash(struct CACHE *cache, unsigned hashid);
struct CACHENODE *cache_find_byindex(struct CACHE *cache, unsigned index);
int cache_do_dependency(
	struct CONTEXT *context,
	struct NODE *node,
	void (*callback)(struct NODE *node, struct CACHENODE *cachenode, void *user),
	void *user);

