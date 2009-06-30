#include <lua.h>
#include <lauxlib.h>

#include "mem.h"
#include "context.h"

const char *CONTEXT_LUA_SCRIPTARGS_TABLE = "_bam_scriptargs";
const char *CONTEXT_LUA_TARGETS_TABLE = "_bam_targets";
const char *CONTEXT_LUA_PATH = "_bam_path";
const char *CONTEXT_LUA_WORKPATH = "_bam_workpath";

/* */
struct CONTEXT *context_get_pointer(lua_State *L)
{
	/* HACK: we store the context pointer as the user data to
		to the allocator for fast access to it */
	void *context;
	lua_getallocf(L, &context);
	return (struct CONTEXT*)context;
}

/*  */
const char *context_get_path(lua_State *L)
{
	const char *path;
	lua_pushstring(L, CONTEXT_LUA_PATH);
	lua_gettable(L, LUA_GLOBALSINDEX);
	path = lua_tostring(L, -1);
	lua_pop(L, 1);
	return path;
}


int context_default_target(struct CONTEXT *context, struct NODE *node)
{
	context->defaulttarget = node;
	return 0;
}
