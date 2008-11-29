#include <time.h>

/* target management */
struct TARGET
{
	struct NODE *node;
	struct TARGET *next;
};

struct CONTEXT
{
	lua_State *lua;
	const char *filename;
	const char *filename_short;
	char script_directory[512];
	
	struct HEAP *heap;
	struct GRAPH *graph;

	struct TARGET *firsttarget;
	struct NODE *defaulttarget;
	
	time_t globaltimestamp;
	
	volatile int current_cmd_num;
	int num_commands;
};

const char *context_get_path(lua_State *L);
struct CONTEXT *context_get_pointer(lua_State *L);
int context_add_target(struct CONTEXT *context, struct NODE *node);
int context_default_target(struct CONTEXT *context, struct NODE *node);
	
extern const char *CONTEXT_LUA_SCRIPTARGS_TABLE;
extern const char *CONTEXT_LUA_TARGETS_TABLE;
extern const char *CONTEXT_LUA_CLONE_TABLE;
extern const char *CONTEXT_LUA_CONTEXT_POINTER;
extern const char *CONTEXT_LUA_PATH;
extern const char *CONTEXT_LUA_WORKPATH;
