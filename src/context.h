#include <time.h>

struct STRINGLIST
{
	struct STRINGLIST *next;
	const char *str;
	size_t len;
};

struct LOOKUP
{
	struct LOOKUP *next;
	struct NODE *node;
	struct STRINGLIST *firstdep;
	struct STRINGLIST *firstpath;
};

struct CONTEXT
{
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
	struct HEAP *lookupheap;
	struct LOOKUP *firstlookup;
	
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
