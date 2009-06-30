#include <time.h>

struct CONTEXT
{
	struct lua_State *lua;
	
	const char *filename;
	const char *filename_short;
	char script_directory[512];
	
	struct HEAP *heap;
	struct GRAPH *graph;
	struct CACHE *cache;

	struct NODE *defaulttarget;
	struct NODE *target;
	
	time_t globaltimestamp;
	
	volatile int current_cmd_num;
	int num_commands;
};


const char *context_get_path(struct lua_State *L);
struct CONTEXT *context_get_pointer(struct lua_State *L);
int context_default_target(struct CONTEXT *context, struct NODE *node);
	
extern const char *CONTEXT_LUA_SCRIPTARGS_TABLE;
extern const char *CONTEXT_LUA_TARGETS_TABLE;
extern const char *CONTEXT_LUA_PATH;
extern const char *CONTEXT_LUA_WORKPATH;
