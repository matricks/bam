#include <stdio.h>
#include <string.h>

#include <stdlib.h> /* malloc */

/* lua includes */
#define LUA_CORE /* make sure that we don't try to import these functions */
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

void build_stringlist(lua_State *L, struct HEAP *heap, struct STRINGLIST **first, int table_index)
{
	struct STRINGLIST *listitem;
	const char *orgstr;
	size_t len;

	int i;
	for(i = 1;; i++)
	{
		/* +1 value */
		lua_rawgeti(L, table_index, i);
		
		if(lua_type(L, -1) == LUA_TNIL)
			break;
			
		/* allocate and fix copy the string */
		orgstr = lua_tolstring(L, -1, &len);
		listitem = (struct STRINGLIST *)mem_allocate(heap, sizeof(struct STRINGLIST) + len + 1);
		listitem->str = (const char *)(listitem+1);
		listitem->len = len;
		memcpy(listitem+1, orgstr, len+1);
		
		/* add it to the list */
		listitem->next = *first;
		*first = listitem;
		
		/* pop value */
		lua_pop(L, 1);
	}
}

/* value for deep walks */
static struct
{
	void (*callback)(lua_State*, void*);
	void *user;
} deepwalkinfo;

static void deep_walk_r(lua_State *L, int table_index)
{
	int i;
	for(i = 1;; i++)
	{
		/* +1 value */
		lua_rawgeti(L, table_index, i);
		
		if(lua_istable(L, -1))
			deep_walk_r(L, lua_gettop(L));
		else if(lua_type(L, -1) == LUA_TSTRING)
			deepwalkinfo.callback(L, deepwalkinfo.user);
		else if(lua_type(L, -1) == LUA_TNIL)
			break;
		else
		{
			/* other value */
			luaL_error(L, "encountered something besides a string or a table");
		}

		/* pop -1 */
		lua_pop(L, 1);
	}

	/* pop -1 */
	lua_pop(L, 1);
}

static void deep_walk(lua_State *L, int start, int stop, void (*callback)(lua_State*, void*), void *user)
{
	int i;
	deepwalkinfo.callback = callback;
	deepwalkinfo.user = user;

	for(i = start; i <= stop; i++)
	{
		if(lua_istable(L, i))
			deep_walk_r(L, i);
		else if(lua_type(L, i) == LUA_TSTRING)
		{
			lua_pushvalue(L, i);
			deepwalkinfo.callback(L, user);
			lua_pop(L, 1);
		}
		else
		{
			luaL_error(L, "encountered something besides a string or a table");
		}
	}
}

/* add_pseudo(string node) */
int lf_add_pseudo(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	int i;
	
	if(lua_gettop(L) != 1)
		luaL_error(L, "add_pseudo: incorrect number of arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	
	/* fetch contexst from lua */
	context = context_get_pointer(L);

	/* create the node */
	i = node_create(&node, context->graph, lua_tostring(L,1), NULL);
	if(i == NODECREATE_NOTNICE)
		luaL_error(L, "add_pseudo: node '%s' is not nice", lua_tostring(L,1));
	else if(i == NODECREATE_EXISTS)
		luaL_error(L, "add_pseudo: node '%s' already exists", lua_tostring(L,1));
	else if(i != NODECREATE_OK)
		luaL_error(L, "add_pseudo: unknown error creating node '%s'", lua_tostring(L,1));
		
	node_set_pseudo(node);
	return 0;
}

/* add_output(string output, string other_output) */
int lf_add_output(lua_State *L)
{
	struct NODE *output;
	struct NODE *other_output;
	struct CONTEXT *context;
	
	if(lua_gettop(L) != 2)
		luaL_error(L, "add_output: incorrect number of arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	luaL_checktype(L, 2, LUA_TSTRING);
	
	context = context_get_pointer(L);

	output = node_find(context->graph, lua_tostring(L,1));
	if(!output)
		luaL_error(L, "add_output: couldn't find node with name '%s'", lua_tostring(L,1));

	other_output = node_get(context->graph, lua_tostring(L,2));
	if(!other_output)
		luaL_error(L, "add_output: couldn't find node with name '%s'", lua_tostring(L,2));
	
	node_add_dependency_withnode(other_output, output);
	node_set_pseudo(other_output);
	return 0;
}

struct NODEATTRIB_CBINFO
{
	struct NODE *node;
	struct NODE *(*callback)(struct NODE*, const char *);
};

static void callback_node_attrib(lua_State *L, void *user)
{
	struct NODEATTRIB_CBINFO *info = (struct NODEATTRIB_CBINFO *)user;
	if(!info->callback(info->node, lua_tostring(L, -1)))
		luaL_error(L, "could not add '%s' to '%s'", lua_tostring(L, -1), lua_tostring(L, 1));
}

/* add_dependency(string node, string dependency) */
static int add_node_attribute(lua_State *L, const char *funcname, struct NODE *(*callback)(struct NODE*, const char *))
{
	struct NODE *node;
	struct CONTEXT *context;
	int n = lua_gettop(L);
	struct NODEATTRIB_CBINFO cbinfo;
	
	if(n < 2)
		luaL_error(L, "%s: to few arguments", funcname);

	luaL_checktype(L, 1, LUA_TSTRING);

	context = context_get_pointer(L);

	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "%s: couldn't find node with name '%s'", funcname, lua_tostring(L,1));
	
	/* seek deps */
	cbinfo.node = node;
	cbinfo.callback = callback;
	deep_walk(L, 2, n, callback_node_attrib, &cbinfo);
	return 0;
}

int lf_add_dependency(lua_State *L) { return add_node_attribute(L, "add_dependency", node_add_dependency); }
int lf_add_constraint_shared(lua_State *L) { return add_node_attribute(L, "add_constraint_shared", node_add_constraint_shared); }
int lf_add_constraint_exclusive(lua_State *L) { return add_node_attribute(L, "add_constraint_exclusive", node_add_constraint_exclusive); }

static void callback_addjob_node(lua_State *L, void *user)
{
	struct JOB *job = (struct JOB *)user;
	struct CONTEXT *context = context_get_pointer(L);
	struct NODE *node;
	const char *filename;
	int i;

	luaL_checktype(L, -1, LUA_TSTRING);
	filename = lua_tostring(L, -1);
	printf("node: %s\n", filename);

	i = node_create(&node, context->graph, filename, job);
	if(i == NODECREATE_NOTNICE)
		luaL_error(L, "add_job: node '%s' is not nice", filename);
	else if(i == NODECREATE_EXISTS)
		luaL_error(L, "add_job: node '%s' already exists", filename);
	else if(i != NODECREATE_OK)
		luaL_error(L, "add_job: unknown error creating node '%s'", filename);
}


static void callback_addjob_deps(lua_State *L, void *user)
{
	struct JOB *job = (struct JOB *)user;
	struct NODELINK *link;
	const char *filename;

	luaL_checktype(L, -1, LUA_TSTRING);
	filename = lua_tostring(L, -1);

	for(link = job->firstoutput; link; link = link->next)
	{
		printf("%s dep %s\n", link->node->filename, filename);
		node_add_dependency(link->node, filename);
	}
}

/* add_job(string/table output, string label, string command, ...) */
int lf_add_job(lua_State *L)
{
	struct JOB *job;
	struct CONTEXT *context;
	
	if(lua_gettop(L) < 3)
		luaL_error(L, "add_job: too few arguments");

	/*luaL_checktype(L, 1, LUA_TSTRING); */
	luaL_checktype(L, 2, LUA_TSTRING);
	luaL_checktype(L, 3, LUA_TSTRING);
	
	/* fetch contexst from lua */
	context = context_get_pointer(L);

	/* create the job */
	job = node_job_create(context->graph, lua_tostring(L,2), lua_tostring(L,3));

	/* create the nodes */
	printf("%d\n", lua_gettop(L));
	deep_walk(L, 1, 1, callback_addjob_node, job);
	printf("%d\n", lua_gettop(L));

	/* seek deps */
	/*cbinfo.node = job;
	cbinfo.callback = node_add_dependency;*/
	deep_walk(L, 4, lua_gettop(L), callback_addjob_deps, job);

	return 0;
}

int lf_set_touch(struct lua_State *L)
{
	struct NODE *node;
	
	if(lua_gettop(L) < 1)
		luaL_error(L, "set_touch: too few arguments");

	luaL_checktype(L, 1, LUA_TSTRING);
	
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
		luaL_error(L, "set_filter: too few arguments");
		
	luaL_checktype(L, 1, LUA_TSTRING);
	luaL_checktype(L, 2, LUA_TSTRING);

	/* find the node */
	node = node_find(context_get_pointer(L)->graph, lua_tostring(L,1));
	if(!node)
		luaL_error(L, "set_filter: couldn't find node with name '%s'", lua_tostring(L,1));

	/* setup the string */	
	str = lua_tolstring(L, 2, &len);
	node->job->filter = (char *)mem_allocate(node->graph->heap, len+1);
	memcpy(node->job->filter, str, len+1);
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
	luaL_checktype(L, 1, LUA_TSTRING);

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
		luaL_error(L, "update_globalstamp: too few arguments");
	luaL_checktype(L, 1, LUA_TSTRING);

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
	luaL_checktype(L, 1, LUA_TSTRING);

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
	*(volatile int*)0 = 0;
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

/* TODO: remove this limit */
#define WALK_MAXDEPTH 32

struct WALKDATA
{
	int index[WALK_MAXDEPTH];
	int depth;
};

static int lf_table_walk_iter(struct lua_State *L)
{
	struct WALKDATA *data;
	int type;
	
	/* 1: walk table  2: last value(ignore) */
	lua_rawgeti(L, 1, 1); /* push 3: the walk data */
	data = (struct WALKDATA *)lua_touserdata(L, -1);
	
	/* 1: walk table  2: last value 3: walk data */
	while(1)
	{
		data->index[data->depth]++;
		
		/* .. 4: current table 5: current value */
		lua_rawgeti(L, 1, data->depth+1); /* push 4: fetch table */
		lua_rawgeti(L, -1, data->index[data->depth]); /* push 5: value in table */
		
		type = lua_type(L, -1);
		
		if(type == LUA_TTABLE)
		{
			data->depth++;
			if(data->depth >= WALK_MAXDEPTH)
				luaL_error(L, "max table depth exceeded");
			data->index[data->depth] = 0;
			lua_rawseti(L, 1, data->depth+1);
			lua_pop(L, 1);
		}
		else if(type == LUA_TNIL)
		{
			/* pop table and nil value */
			lua_pop(L, 2);
			data->depth--;
			if(data->depth == 0)
			{
				lua_pushnil(L);
				return 1;
			}
		}
		else if(type == LUA_TSTRING)
		{
			lua_pushvalue(L, 1); /* push the table stack again */
			return 2;
		}
		else
			luaL_error(L, "tablewalk: encountered strange value in tables");
	}
}

/*
	the walk table looks like this
	t = {
			[1] = walk data
			[2] = table 1
			[3] = table 2
			[N] = table N
	}
*/
int lf_table_walk(struct lua_State *L)
{
	struct WALKDATA *data;

	if(lua_gettop(L) != 1)
		luaL_error(L, "table_walk: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TTABLE);
	
	/* 1: table to iterate over */
	lua_pushcfunction(L, lf_table_walk_iter); /* 2: iterator function */
	lua_createtable(L, 4, 0); /* 3: table stack */
	
	data = (struct WALKDATA *)lua_newuserdata(L, sizeof(struct WALKDATA));
	data->depth = 1;
	data->index[data->depth] = 0;
	lua_rawseti(L, 3, 1);
	
	lua_pushvalue(L, 1);
	lua_rawseti(L, 3, 2);
	lua_pushnil(L);
	
	return 3;
}

/* does a deep copy of a table */
static int table_deepcopy_r(struct lua_State *L)
{
	size_t s;
	
	/* 1: table to copy, 2: new table */
	s = lua_objlen(L, -1);
	lua_createtable(L, 0, s);
	
	/* 3: iterator */
	lua_pushnil(L);
	while(lua_next(L, -3))
	{
		/* 4: value */
		if(lua_istable(L, -1))
		{
			table_deepcopy_r(L); /* 5: new table */
			lua_pushvalue(L, -3); /* 6: key */
			lua_pushvalue(L, -2); /* 7: value */
			lua_settable(L, -6); /* pops 6 and 7 */
			lua_pop(L, 1); /* pops 5 */
		}
		else
		{
			lua_pushvalue(L, -2); /* 5: key */
			lua_pushvalue(L, -2); /* 6: value */
			lua_settable(L, -5); /* pops 5 and 6 */
		}
		
		/* pops 4 */
		lua_pop(L, 1);
	}

	
	/* transfer the meta table */
	if(lua_getmetatable(L, -2))
		lua_setmetatable(L, -2);
	
	return 1;
}

int lf_table_deepcopy(struct lua_State *L)
{
	if(lua_gettop(L) != 1)
		luaL_error(L, "table_deepcopy: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TTABLE);
	
	return table_deepcopy_r(L);
}

static int flatten_index;

/* flattens a table into a simple table with strings */
static int lf_table_flatten_r(struct lua_State *L, int table_index)
{	
	/* +1: iterator */
	lua_pushnil(L);
	while(lua_next(L, table_index))
	{
		/* +2: value */
		if(lua_istable(L, -1))
			lf_table_flatten_r(L, lua_gettop(L));
		else if(lua_type(L, -1) == LUA_TSTRING)
		{
			lua_pushnumber(L, flatten_index); /* +3: key */
			lua_pushvalue(L, -2); /* +4: value */
			lua_settable(L, 2); /* pops +3 and +4 */
			
			flatten_index++;
		}
		else
		{
			/* other value */
			luaL_error(L, "encountered something besides a string or a table");
		}
		
		/* pops +2 */
		lua_pop(L, 1);
	}
	
	return 1;
}

int lf_table_flatten(struct lua_State *L)
{
	size_t s;
	
	if(lua_gettop(L) != 1)
		luaL_error(L, "table_flatten: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TTABLE);
		
	/* 1: table to copy, 2: new table */
	s = lua_objlen(L, -1);
	flatten_index = 1;
	lua_createtable(L, 0, s);
	lf_table_flatten_r(L, 1);
	return 1;
}

static char string_buffer[1024*4];

int lf_table_tostring(struct lua_State *L)
{
	/* 1: table 2: prefix, 3: postfix */
	size_t prefix_len, postfix_len;
	size_t total_len = 0;
	size_t item_len = 0;
	const char *prefix;
	const char *postfix;
	char *buffer;
	char *current;
	const char *item;
	
	if(lua_gettop(L) != 3)
		luaL_error(L, "table_tostring: incorrect number of arguments");
	
	luaL_checktype(L, 1, LUA_TTABLE);

	prefix = lua_tolstring(L, 2, &prefix_len);
	postfix = lua_tolstring(L, 3, &postfix_len);
	
	/* first, figure out the total size */
	
	/* 4: iterator */
	lua_pushnil(L);
	while(lua_next(L, 1))
	{
		/* 5: value */
		if(lua_type(L, -1) == LUA_TSTRING)
		{
			lua_tolstring(L, -1, &item_len);
			total_len += prefix_len+item_len+postfix_len;
		}
		
		/* pops 5 */
		lua_pop(L, 1);		
	}
	
	/* now allocate the buffer and start building the complete string */
	if(total_len < sizeof(string_buffer))
		buffer = string_buffer;
	else
		buffer = malloc(total_len);
		
	current = buffer;

	/* 4: iterator */
	lua_pushnil(L);
	while(lua_next(L, 1))
	{
		/* 5: value */
		if(lua_type(L, -1) == LUA_TSTRING)
		{
			item = lua_tolstring(L, -1, &item_len);
			memcpy(current, prefix, prefix_len); current += prefix_len;
			memcpy(current, item, item_len); current += item_len;
			memcpy(current, postfix, postfix_len); current += postfix_len;
		}
		
		/* pops 5 */
		lua_pop(L, 1);		
	}
	
	/* push the new string onto the stack and clean up */
	lua_pushlstring(L, buffer, total_len);
	if(buffer != string_buffer)
		free(buffer);
	
	return 1;
}

int lf_loadplugin(struct lua_State *L)
{
	PLUGINFUNC func;
	if(lua_gettop(L) != 1)
		luaL_error(L, "loadplugin: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TSTRING);

	func = plugin_load(lua_tostring(L, 1));
	if(!func)
		luaL_error(L, "loadplugin: error loading plugin");
		
	lua_settop(L, 0);
	if((*func)(L) != 0)
		luaL_error(L, "loadplugin: error initing plugin");
	
	return 0;
}
