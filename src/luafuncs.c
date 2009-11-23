#include <stdio.h>
#include <string.h>

/* lua includes */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "node.h"
#include "context.h"
#include "support.h"
#include "luafuncs.h"
#include "path.h"
#include "mem.h"
#include "session.h"

/* add_job(string output, string label, string command) */
int lf_add_job(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	int i;
	if(lua_gettop(L) != 3)
		luaL_error(L, "add_job: incorrect number of arguments");

	/* fetch contexst from lua */
	context = context_get_pointer(L);

	/* create the node */
	i = node_create(&node, context->graph, lua_tostring(L,1), lua_tostring(L,2), lua_tostring(L,3));
	if(i == NODECREATE_NOTNICE)
		luaL_error(L, "add_job: node '%s' is not nice", lua_tostring(L,1));
	else if(i == NODECREATE_EXISTS)
		luaL_error(L, "add_job: node '%s' already exists", lua_tostring(L,1));
	else if(i != NODECREATE_OK)
		luaL_error(L, "add_job: unknown error creating node '%s'", lua_tostring(L,1));

	return 0;
}

/* add_dependency(string node, string dependency) */
int lf_add_dependency(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	int n = lua_gettop(L);
	int i;
	
	if(n < 2)
		luaL_error(L, "add_dep: to few arguments");

	context = context_get_pointer(L);

	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "add_dep: couldn't find node with name '%s'", lua_tostring(L,1));
	
	/* seek deps */
	for(i = 2; i <= n; ++i)
	{
		if(lua_isstring(L,n))
		{
			if(!node_add_dependency(node, lua_tostring(L,n)))
				luaL_error(L, "add_dep: could not add dependency for node '%s'", lua_tostring(L,1));
		}
		else
			luaL_error(L, "add_dep: dependency is not a string for node '%s'", lua_tostring(L,1));
	}
	
	return 0;
}

int lf_set_touch(struct lua_State *L)
{
	struct NODE *node;
	
	if(lua_gettop(L) < 1)
		luaL_error(L, "set_touch: to few arguments");

	node = node_find(context_get_pointer(L)->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "set_touch: couldn't find node with name '%s'", lua_tostring(L,1));
		
	node->touch = 1;
	return 0;
}


int lf_set_filter(struct lua_State *L)
{
	struct NODE *node;
	const char *str;
	size_t len;
	
	/* check the arguments */
	if(lua_gettop(L) < 2)
		luaL_error(L, "set_filter: to few arguments");
	if(lua_type(L,2) != LUA_TSTRING)
		luaL_error(L, "set_filter: expected string as second argument");

	/* find the node */
	node = node_find(context_get_pointer(L)->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "set_filter: couldn't find node with name '%s'", lua_tostring(L,1));

	/* setup the string */	
	str = lua_tolstring(L, 2, &len);
	node->filter = (char *)mem_allocate(node->graph->heap, len+1);
	memcpy(node->filter, str, len+1);
	return 0;
}


static void build_stringlist(lua_State *L, struct HEAP *heap, struct STRINGLIST **first, int tableindex)
{
	struct STRINGLIST *listitem;
	const char *orgstr;
	size_t len;

	lua_pushnil(L);
	while (lua_next(L, tableindex) != 0)
	{
		/* allocate and fix copy the string */
		orgstr = lua_tolstring(L, -1, &len);
		listitem = (struct STRINGLIST *)mem_allocate(heap, sizeof(struct STRINGLIST) + len + 1);
		listitem->str = (const char *)(listitem+1);
		listitem->len = len;
		memcpy(listitem+1, orgstr, len+1);
		
		/* add it to the list */
		listitem->next = *first;
		*first = listitem;
		
		/* pop value, keep key for iteration */
		lua_pop(L, 1);
	}
}

/* add_dependency(string node, table paths, table deps) */
int lf_add_dependency_search(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	struct LOOKUP *lookup;
	
	if(lua_gettop(L) != 3)
		luaL_error(L, "add_dep_search: expected 3 arguments");

	context = context_get_pointer(L);

	/* check all parameters */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "add_dep_search: couldn't find node with name '%s'", lua_tostring(L,1));
	if(lua_type(L,2) != LUA_TTABLE)
		luaL_error(L, "add_dep_search: expected table as second argument");
	if(lua_type(L,3) != LUA_TTABLE)
		luaL_error(L, "add_dep_search: expected table as third argument");
	
	/* allocate the lookup */
	lookup = (struct LOOKUP*)mem_allocate(context->lookupheap, sizeof(struct LOOKUP));
	lookup->node = node;
	lookup->firstpath = NULL;
	lookup->firstdep = NULL;
	
	/* build the string lists */
	build_stringlist(L, context->lookupheap, &lookup->firstpath, 2);
	build_stringlist(L, context->lookupheap, &lookup->firstdep, 3);
	
	/* link it in */
	lookup->next = context->firstlookup;
	context->firstlookup = lookup;
	
	return 0;
}


/* default_target(string filename) */
int lf_default_target(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;

	int n = lua_gettop(L);
	if(n != 1)
		luaL_error(L, "default_target: incorrect number of arguments");
	
	if(!lua_isstring(L,1))
		luaL_error(L, "default_target: expected string");

	/* fetch context from lua */
	context = context_get_pointer(L);

	/* search for the node */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "default_target: node '%s' not found", lua_tostring(L,1));
	
	/* set target */
	context_default_target(context, node);
	return 0;
}

/* update_globalstamp(string filename) */
int lf_update_globalstamp(lua_State *L)
{
	struct CONTEXT *context;
	time_t file_stamp;
	
	if(lua_gettop(L) < 1)
		luaL_error(L, "update_globalstamp: to few arguments");

	context = context_get_pointer(L);
	file_stamp = file_timestamp(lua_tostring(L,1)); /* update global timestamp */
	
	if(file_stamp > context->globaltimestamp)
		context->globaltimestamp = file_stamp;
	
	return 0;
}


/* loadfile(filename) */
int lf_loadfile(lua_State *L)
{
	if(lua_gettop(L) < 1)
		luaL_error(L, "loadfile: too few arguments");

	if(session.verbose)
		printf("%s: reading script from '%s'\n", session.name, lua_tostring(L,1));
	
	if(luaL_loadfile(L, lua_tostring(L,1)) != 0)
		lua_error(L);
	return 1;
}


/* ** */
static void debug_print_lua_value(lua_State *L, int i)
{
	if(lua_type(L,i) == LUA_TNIL)
		printf("nil");
	else if(lua_type(L,i) == LUA_TSTRING)
		printf("'%s'", lua_tostring(L,i));
	else if(lua_type(L,i) == LUA_TNUMBER)
		printf("%f", lua_tonumber(L,i));
	else if(lua_type(L,i) == LUA_TBOOLEAN)
	{
		if(lua_toboolean(L,i))
			printf("true");
		else
			printf("false");
	}
	else if(lua_type(L,i) == LUA_TTABLE)
	{
		printf("{...}");
	}
	else
		printf("%p (%s (%d))", lua_topointer(L,i), lua_typename(L,lua_type(L,i)), lua_type(L,i));
}


/* error function */
int lf_errorfunc(lua_State *L)
{
	int depth = 0;
	int frameskip = 1;
	lua_Debug frame;

	if(session.report_color)
		printf("\033[01;31m%s\033[00m\n", lua_tostring(L,-1));
	else
		printf("%s\n", lua_tostring(L,-1));
	
	if(session.lua_backtrace)
	{
		printf("backtrace:\n");
		while(lua_getstack(L, depth, &frame) == 1)
		{
			depth++;
			
			lua_getinfo(L, "nlSf", &frame);

			/* check for functions that just report errors. these frames just confuses more then they help */
			if(frameskip && strcmp(frame.short_src, "[C]") == 0 && frame.currentline == -1)
				continue;
			frameskip = 0;
			
			/* print stack frame */
			printf("  %s(%d): %s %s\n", frame.short_src, frame.currentline, frame.name, frame.namewhat);
			
			/* print all local variables for the frame */
			if(session.lua_locals)
			{
				int i;
				const char *name = 0;
				
				i = 1;
				while((name = lua_getlocal(L, &frame, i)) != NULL)
				{
					printf("    %s = ", name);
					debug_print_lua_value(L,-1);
					printf("\n");
					lua_pop(L,1);
					i++;
				}
				
				i = 1;
				while((name = lua_getupvalue(L, -1, i)) != NULL)
				{
					printf("    upvalue #%d: %s ", i-1, name);
					debug_print_lua_value(L, -1);
					lua_pop(L,1);
					i++;
				}
			}
		}
	}
	
	return 1;
}

int lf_panicfunc(lua_State *L)
{
	printf("%s: PANIC! I'm gonna segfault now\n", session.name);
	*(int*)0 = 0;
	return 0;
}

int lf_mkdir(struct lua_State *L)
{
	if(lua_gettop(L) < 1)
		luaL_error(L, "mkdir: too few arguments");	

	if(!lua_isstring(L,1))
		luaL_error(L, "mkdir: expected string");
		
	if(file_createdir(lua_tostring(L,1)) == 0)
		lua_pushnumber(L, 1);	
	else
		lua_pushnil(L);
	return 1;
}


int lf_fileexist(struct lua_State *L)
{
	if(lua_gettop(L) < 1)
		luaL_error(L, "fileexist: too few arguments");	

	if(!lua_isstring(L,1))
		luaL_error(L, "fileexist: expected string");
		
	if(file_exist(lua_tostring(L,1)))
		lua_pushnumber(L, 1);	
	else
		lua_pushnil(L);
	return 1;
}

int lf_istable(lua_State *L)
{
	if(lua_type(L,-1) == LUA_TTABLE)
		lua_pushnumber(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

int lf_isstring(lua_State *L)
{
	if(lua_type(L,-1) == LUA_TSTRING)
		lua_pushnumber(L, 1);
	else
		lua_pushnil(L);
	return 1;
}


