#include <time.h>

struct CONTEXT;

struct DEFERRED
{
	struct DEFERRED *next;
	struct NODE *node;
	int (*run)(struct CONTEXT *context, struct DEFERRED *info);
	void *user;
};

struct CONTEXT
{
	struct HEAP *luaheap;
	struct lua_State *lua;
	
	const char *filename;
	const char *filename_short;
	char script_directory[512];
	
	struct HEAP *graphheap;
	struct GRAPH *graph;
	struct CACHE *cache;

	struct NODE *defaulttarget;
	struct NODE *target;

	/* this heap is used for dependency lookups that has to happen after we 
		parsed the whole file */
	struct HEAP *deferredheap;
	struct DEFERRED *firstdeferred;
	
	time_t globaltimestamp;
	time_t buildtime;

	int exit_on_error;
	int num_commands;
	volatile int errorcode;
	volatile int current_cmd_num;
};

const char *context_get_path(struct lua_State *L);
struct CONTEXT *context_get_pointer(struct lua_State *L);
int context_default_target(struct CONTEXT *context, struct NODE *node);

int context_build_prepare(struct CONTEXT *context);
int context_build_clean(struct CONTEXT *context);
int context_build_make(struct CONTEXT *context);

extern const char *CONTEXT_LUA_SCRIPTARGS_TABLE;
extern const char *CONTEXT_LUA_TARGETS_TABLE;
extern const char *CONTEXT_LUA_PATH;
extern const char *CONTEXT_LUA_WORKPATH;
