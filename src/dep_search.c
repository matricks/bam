/*#include <stdio.h>
#include <stdlib.h>
#include <string.h>*/

#define LUA_CORE /* make sure that we don't try to import these functions */
#include <lua.h>
#include <lauxlib.h>

#include "path.h"
#include "context.h"
#include "mem.h"
#include "node.h"
#include "support.h"
#include "luafuncs.h"

struct DEPPLAIN
{
	struct STRINGLIST *firstpath;
	struct STRINGLIST *firstdep;
};

/* returns 0 on no find, 1 on find and -1 on error */
static int checkpath(struct CONTEXT *context, struct NODE *node, const char *path)
{
	struct NODE *depnode;
	time_t stamp;

	/* search up the node and add it if we need */
	depnode = node_find(context->graph, path);
	if(depnode)
	{
		if(!node_add_dependency (node, depnode))
			return -1;
		return 1;
	}
	
	/* check if it exists on the disk */
	stamp = file_timestamp(path);
	if(stamp)
	{
		node_create(&depnode, node->graph, path, NULL, stamp);
		if(!node_add_dependency (node, depnode))
			return -1;
		return 1;
	}
	
	return 0;
}

/* this functions takes the whole deferred lookup list and searches for the file */
static int do_run(struct CONTEXT *context, struct DEFERRED *info)
{
	struct STRINGLIST *dep;
	struct STRINGLIST *path;
	struct DEPPLAIN *plain = (struct DEPPLAIN *)info->user;
	char buffer[MAX_PATH_LENGTH];
	int result;
	
	for(dep = plain->firstdep; dep; dep = dep->next)
	{
		/* check the current directory */
		result = checkpath(context, info->node, dep->str);
		if(result == 1)
			continue;
		if(result == -1)
			return -1;

		/* check all the other directories */
		for(path = plain->firstpath; path; path = path->next)
		{
			/* build the path, "%s/%s" */
			if(path_join(path->str, path->len, dep->str, dep->len, buffer, sizeof(buffer)) != 0)
				return -1;
			
			result = checkpath(context, info->node, buffer);
			if(result == 1)
				break;
			if(result == -1)
				return -1;
		}
	}
	
	return 0;
}

/* add_dependency_search(string node, table paths, table deps) */
int lf_add_dependency_search(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	struct DEFERRED *deferred;
	struct DEPPLAIN *plain;
	
	if(lua_gettop(L) != 3)
		luaL_error(L, "add_dep_search: expected 3 arguments");
	luaL_checktype(L, 1, LUA_TSTRING);

	context = context_get_pointer(L);

	/* check all parameters */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "add_dep_search: couldn't find node with name '%s'", lua_tostring(L,1));
	if(lua_type(L,2) != LUA_TTABLE)
		luaL_error(L, "add_dep_search: expected table as second argument");
	if(lua_type(L,3) != LUA_TTABLE)
		luaL_error(L, "add_dep_search: expected table as third argument");
		
	deferred = (struct DEFERRED *)mem_allocate(context->deferredheap, sizeof(struct DEFERRED));
	plain = (struct DEPPLAIN *)mem_allocate(context->deferredheap, sizeof(struct DEPPLAIN));
	
	deferred->node = node;
	deferred->user = plain;
	deferred->run = do_run;
	deferred->next = context->firstdeferred_search;
	context->firstdeferred_search = deferred;
		
	/* allocate the lookup */
	plain->firstpath = NULL;
	plain->firstdep = NULL;
	
	/* build the string lists */
	build_stringlist(L, context->deferredheap, &plain->firstpath, 2);
	build_stringlist(L, context->deferredheap, &plain->firstdep, 3);
	
	return 0;
}
