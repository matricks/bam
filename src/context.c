#include <lua.h>
#include <lauxlib.h>

#include "mem.h"
#include "context.h"

const char *CONTEXT_LUA_SCRIPTARGS_TABLE = "_bam_scriptargs";
const char *CONTEXT_LUA_TARGETS_TABLE = "_bam_targets";
const char *CONTEXT_LUA_CLONE_TABLE = "_bam_clone";
const char *CONTEXT_LUA_CONTEXT_POINTER = "_bam_context";
const char *CONTEXT_LUA_PATH = "_bam_path";
const char *CONTEXT_LUA_WORKPATH = "_bam_workpath";

/* */
struct CONTEXT *context_get_pointer(lua_State *L)
{
	struct CONTEXT *context;
	lua_pushstring(L, CONTEXT_LUA_CONTEXT_POINTER);
	lua_gettable(L, LUA_GLOBALSINDEX);
	context = (struct CONTEXT *)lua_topointer(L, -1);
	lua_pop(L, 1);
	return context;
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

/* */
int context_add_target(struct CONTEXT *context, struct NODE *node)
{
	struct TARGET *target;

	/* search for target */
	target = context->firsttarget;
	while(target)
	{
		if(target->node == node)
			return 1;
		target = target->next;
	}
	
	target = (struct TARGET *)mem_allocate(context->heap, sizeof(struct TARGET));
	target->node = node; 
	target->next = context->firsttarget;
	context->firsttarget = target;
	
	/* set default target if no other target exist */
	if(!context->defaulttarget)
		context->defaulttarget = node;
	
	return 0;
}

int context_default_target(struct CONTEXT *context, struct NODE *node)
{
	context->defaulttarget = node;
	return 0;
}
