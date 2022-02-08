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
#include "dep.h"

/* constants */
const char *CONTEXT_LUA_SCRIPTARGS_TABLE = "_bam_scriptargs";
const char *CONTEXT_LUA_TARGETS_TABLE = "_bam_targets";
const char *CONTEXT_LUA_PATH = "_bam_path";
const char *CONTEXT_LUA_WORKPATH = "_bam_workpath";

/* context helper functions */
static struct CONTEXT *context_get_pointer(lua_State *L)
{
	/* HACK: we store the context pointer as the user data to
		to the allocator for fast access to it */
	void *context;
	lua_getallocf(L, &context);
	return (struct CONTEXT*)context;
}

/*  */
static const char *context_get_path(lua_State *L)
{
	const char *path;
	lua_pushstring(L, CONTEXT_LUA_PATH);
	lua_gettable(L, LUA_REGISTRYINDEX);
	path = lua_tostring(L, -1);
	lua_pop(L, 1);
	return path;
}


static const char *curfuncname(lua_State *L)
{
	lua_Debug ar;
	if (!lua_getstack(L, 0, &ar))  /* no stack frame? */
		return "?";
	lua_getinfo(L, "n", &ar);
	if (ar.name == NULL)
		return "?";
	return ar.name;
}

static void luaL_checknumarg_eq(lua_State *L, int expect)
{
	int n = lua_gettop(L);
	if(n != expect)
		luaL_error(L, "bad number of arguments to '%s', expected %d, got %d", curfuncname(L), expect, n );
}

static void luaL_checknumarg_ge(lua_State *L, int expect)
{
	int n = lua_gettop(L);
	if(n < expect)
		luaL_error(L, "bad number of arguments to '%s', expected %d or more, got %d", curfuncname(L), expect, n );
}

static struct NODE *node_get_or_fail(lua_State *L, struct GRAPH *graph, const char *filename)
{
	struct NODE *node = node_get(graph, filename);
	if(node == NULL)
		luaL_error(L, "node '%s' cloud not be created", filename);
	return node;
}

static void build_stringlist(lua_State *L, struct HEAP *heap, struct STRINGLIST **first, int table_index)
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
		if (!orgstr) continue;
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

static void deep_walk_r(lua_State *L, int narg, int table_index)
{
	int i;
	for(i = 1;; i++)
	{
		/* +1 value */
		lua_rawgeti(L, table_index, i);
		
		if(lua_istable(L, -1))
			deep_walk_r(L, narg, lua_gettop(L));
		else if(lua_type(L, -1) == LUA_TSTRING)
			deepwalkinfo.callback(L, deepwalkinfo.user);
		else if(lua_type(L, -1) == LUA_TNIL)
			break;
		else
		{
			/* other value */
			luaL_argerror(L, i, lua_pushfstring(L, "table structure contains a %s", luaL_typename(L, -1)));
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
			deep_walk_r(L, i, i);
		else if(lua_type(L, i) == LUA_TSTRING)
		{
			lua_pushvalue(L, i);
			deepwalkinfo.callback(L, user);
			lua_pop(L, 1);
		}
		else
		{
			luaL_argerror(L, i, "not a string or table");
		}
	}
}

static void handle_node_errors(lua_State *L, int errorcode, int narg)
{
	if(errorcode == NODECREATE_NOTNICE)
		luaL_argerror(L, narg, lua_pushfstring(L, "node '%s' is not nice", lua_tostring(L, narg)));
	else if(errorcode == NODECREATE_EXISTS)
		luaL_argerror(L, narg, lua_pushfstring(L, "node '%s' already exists", lua_tostring(L, narg)));
	else if(errorcode != NODECREATE_OK)
		luaL_argerror(L, narg, lua_pushfstring(L, "unknown error creating node '%s'", lua_tostring(L,narg)));
}

static struct NODE *luaL_checknode(lua_State *L, struct CONTEXT *context, int narg)
{
	struct NODE *node = node_find(context->graph, luaL_checklstring(L,narg,NULL));
	if(!node)
		luaL_argerror(L, narg, lua_pushfstring(L, "node '%s' does not exist", lua_tostring(L, narg)));
	return node;
}

static struct NODE *luaL_checkjobnode(lua_State *L, struct CONTEXT *context, int narg)
{
	struct NODE *node = luaL_checknode(L, context, narg);
	if(!node->job->cmdline)
		luaL_argerror(L, narg, lua_pushfstring(L, "node '%s' is not a job", lua_tostring(L, narg)));
	return node;
}

/* add_pseudo(string node) */
int lf_add_pseudo(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	const char *name;
	int i;
	
	luaL_checknumarg_eq(L, 1);
	name = luaL_checklstring(L, 1, NULL);
	
	/* fetch contexst from lua */
	context = context_get_pointer(L);

	/* create the node */
	i = node_create(&node, context->graph, name, NULL, TIMESTAMP_PSEUDO);
	handle_node_errors(L, i, 1);
	return 0;
}

/* add_output(string output, string other_output) */
int lf_add_output(lua_State *L)
{
	struct NODE *output;
	struct NODE *other_output;
	struct CONTEXT *context = context_get_pointer(L);
	int i;
	const char *filename;
	
	luaL_checknumarg_eq(L, 2);

	output = luaL_checkjobnode(L, context, 1);
	filename = luaL_checklstring(L, 2, NULL);

	i = node_create(&other_output, context->graph, filename, output->job, TIMESTAMP_NONE);
	handle_node_errors(L, i, 2);

	node_inherit_dependencies(other_output, output);
	
	return 0;
}

struct NODEATTRIBSTRING_CBINFO
{
	struct NODE *node;
	int (*callback)(struct NODE*, const char * filename);
};

static void callback_node_attrib_string(lua_State *L, void *user)
{
	struct NODEATTRIBSTRING_CBINFO *info = (struct NODEATTRIBSTRING_CBINFO *)user;
	const char *othername = lua_tostring(L, -1);
	if(info->callback(info->node, othername))
		luaL_error(L, "%s: could not add '%s' to '%s'", curfuncname(L), othername, lua_tostring(L, 1));
}

/* add_dependency(string node, string dependency) */
static int add_node_attribute_string(lua_State *L, const char *funcname, int (*callback)(struct NODE*, const char * filename))
{
	struct NODE *node;
	struct CONTEXT *context;
	struct NODEATTRIBSTRING_CBINFO cbinfo;
	
	context = context_get_pointer(L);
	node = luaL_checkjobnode(L, context, 1);
	
	/* seek deps */
	cbinfo.node = node;
	cbinfo.callback = callback;
	deep_walk(L, 2, lua_gettop(L), callback_node_attrib_string, &cbinfo);
	return 0;
}

int lf_add_sideeffect(lua_State *L) { return add_node_attribute_string(L, "add_sideeffect", node_add_sideeffect ); }
int lf_add_clean(lua_State *L) { return add_node_attribute_string(L, "add_clean", node_add_clean ); }

struct NODEATTRIB_CBINFO
{
	struct NODE *node;
	struct NODE *(*callback)(struct NODE*, struct NODE*);
};

static void callback_node_attrib(lua_State *L, void *user)
{
	struct NODEATTRIB_CBINFO *info = (struct NODEATTRIB_CBINFO *)user;
	const char *othername = lua_tostring(L, -1);
	struct NODE *othernode = node_get_or_fail(L, info->node->graph, othername);
	if(!info->callback(info->node, othernode))
		luaL_error(L, "%s: could not add '%s' to '%s'", curfuncname(L), othername, lua_tostring(L, 1));
}

/* add_dependency(string node, string dependency) */
static int add_node_attribute(lua_State *L, const char *funcname, struct NODE *(*callback)(struct NODE*, struct NODE*))
{
	struct NODE *node;
	struct CONTEXT *context;
	struct NODEATTRIB_CBINFO cbinfo;
	
	context = context_get_pointer(L);
	node = luaL_checknode(L, context, 1);
	
	/* seek deps */
	cbinfo.node = node;
	cbinfo.callback = callback;
	deep_walk(L, 2, lua_gettop(L), callback_node_attrib, &cbinfo);
	return 0;
}

int lf_add_dependency(lua_State *L) { return add_node_attribute(L, "add_dependency", node_add_dependency ); }
int lf_add_constraint_shared(lua_State *L) { return add_node_attribute(L, "add_constraint_shared", node_add_constraint_shared ); }
int lf_add_constraint_exclusive(lua_State *L) { return add_node_attribute(L, "add_constraint_exclusive", node_add_constraint_exclusive ); }

static void callback_addjob_node(lua_State *L, void *user)
{
	struct JOB *job = (struct JOB *)user;
	struct CONTEXT *context = context_get_pointer(L);
	struct NODE *node;
	const char *filename;
	int i;

	filename = luaL_checklstring(L, -1, NULL);

	i = node_create(&node, context->graph, filename, job, TIMESTAMP_NONE);
	handle_node_errors(L, i, 1); /* TODO: FIX THIS */
}


static void callback_addjob_deps(lua_State *L, void *user)
{
	struct JOB *job = (struct JOB *)user;
	struct NODELINK *link;
	const char *filename;

	filename = luaL_checklstring(L, -1, NULL);

	for(link = job->firstoutput; link; link = link->next)
		node_add_dependency (link->node, node_get_or_fail(L, link->node->graph, filename));
}

/* add_job(string/table output, string label, string command, ...) */
int lf_add_job(lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	struct JOB *job;

	luaL_checknumarg_ge(L, 3);
	
	/* create the job */
	job = node_job_create(context->graph, luaL_checklstring(L, 2, NULL), luaL_checklstring(L, 3, NULL));

	/* create the nodes */
	deep_walk(L, 1, 1, callback_addjob_node, job);

	/* seek deps */
	deep_walk(L, 4, lua_gettop(L), callback_addjob_deps, job);

	return 0;
}

int lf_set_filter(struct lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	struct NODE *node;
	const char *str;
	size_t len;
	
	luaL_checknumarg_eq(L, 2);

	/* find the node */
	node = luaL_checknode(L, context, 1);

	/* setup the string */	
	str = luaL_checklstring(L, 2, &len);
	node->job->filter = (char *)mem_allocate(node->graph->heap, len+1);
	memcpy(node->job->filter, str, len+1);
	return 0;
}

/* nodeexist(string nodename) */
int lf_nodeexist(struct lua_State *L)
{
	struct NODE *node;
	luaL_checknumarg_eq(L, 1);
	node = node_find(context_get_pointer(L)->graph, luaL_checklstring(L,1,NULL));
	if(node)
		lua_pushboolean(L, 1);
	else
		lua_pushboolean(L, 0);
	return 1;
}

/* isoutput(string nodename) */
int lf_isoutput(struct lua_State *L)
{
	struct NODE *node;
	luaL_checknumarg_eq(L, 1);
	node = node_find(context_get_pointer(L)->graph, luaL_checklstring(L,1,NULL));
	if(!node)
		lua_pushboolean(L, 0);
	else
	{
		if(node->job->cmdline)
			lua_pushboolean(L, 1);
		else
			lua_pushboolean(L, 0);
	}
	return 1;
}


/* lf_set_priority(string nodename, prio) */
int lf_set_priority(struct lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	luaL_checknumarg_eq(L, 2);
	luaL_checkjobnode(L, context, 1)->job->priority = luaL_checkinteger(L, 2);
	return 1;
}


/* lf_modify_priority(string nodename, prio) */
int lf_modify_priority(struct lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	luaL_checknumarg_eq(L, 2);
	luaL_checkjobnode(L, context, 1)->job->priority += luaL_checkinteger(L, 2);
	return 1;
}

/* lf_skip_output_verification(string nodename) */
int lf_skip_output_verification(struct lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	luaL_checknumarg_eq(L, 1);
	luaL_checknode(L, context, 1)->skipverifyoutput = 1;
	return 1;
}


/* default_target(string filename) */
int lf_default_target(lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);

	luaL_checknumarg_eq(L, 1);

	/* set target */
	context_default_target(context, luaL_checknode(L, context, 1));
	return 0;
}

/* update_globalstamp(string filename) */
int lf_update_globalstamp(lua_State *L)
{
	struct CONTEXT *context = context_get_pointer(L);
	time_t file_stamp;

	luaL_checknumarg_eq(L, 1);

	file_stamp = file_timestamp(luaL_checklstring(L,1,NULL)); /* update global timestamp */
	
	if(file_stamp > context->globaltimestamp)
		context->globaltimestamp = file_stamp;
	
	return 0;
}


/* loadfile(filename) */
int lf_loadfile(lua_State *L)
{
	luaL_checknumarg_eq(L, 1);

	if(session.verbose)
		printf("%s: reading script from '%s'\n", session.name, luaL_checklstring(L,1,NULL));
	
	if(luaL_loadfile(L, luaL_checklstring(L,1,NULL)) != 0)
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
	const char* error_str = NULL;

	/* Due to how luaB_error works we only get the source file/line number if a string is passed to error() in lua. 
	 If not, we get null from lua_tostring(L,-1);, wich crashes some libc in printf (like glibc, at least in some cases).*/
	error_str = lua_tostring(L,-1);
	if(!error_str)
		error_str = "error";

	if(session.report_color)
		printf("\033[01;31m%s\033[00m\n", error_str);
	else
		printf("%s\n", error_str);
	
	if(session.lua_backtrace)
	{
		printf("backtrace:\n");
		while(lua_getstack(L, depth, &frame) == 1)
		{
			const char* frame_name = NULL;
			depth++;
			
			lua_getinfo(L, "nlSf", &frame);

			/* check for functions that just report errors. these frames just confuses more then they help */
			if(frameskip && strcmp(frame.short_src, "[C]") == 0 && frame.currentline == -1)
				continue;
			frameskip = 0;

			/* according to the documentation frame.name may be NULL */
			frame_name = frame.name;
			if(!frame_name)
				frame_name = "";

			/* print stack frame */
			printf("  %s(%d): %s %s\n", frame.short_src, frame.currentline, frame_name, frame.namewhat);
			
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
	luaL_checknumarg_eq(L, 1);
	if(file_createdir(luaL_checklstring(L,1,NULL)) == 0)
		lua_pushboolean(L, 1);	
	else
		lua_pushnil(L);
	return 1;
}

int lf_mkdirs(struct lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	if(file_createpath(luaL_checklstring(L,1,NULL)) == 0)
		lua_pushboolean(L, 1);	
	else
		lua_pushnil(L);
	return 1;
}

int lf_fileexist(struct lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	if(file_timestamp(luaL_checklstring(L,1,NULL)))
		lua_pushboolean(L, 1);	
	else
		lua_pushnil(L);
	return 1;
}


int lf_isfile(struct lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	if( file_isregular(luaL_checklstring(L,1,NULL)))
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

int lf_isdir(struct lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	if( file_isdir(luaL_checklstring(L,1,NULL)))
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
	return 1;
}


int lf_istable(lua_State *L)
{
	if(lua_type(L,-1) == LUA_TTABLE)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

int lf_isstring(lua_State *L)
{
	if(lua_type(L,-1) == LUA_TSTRING)
		lua_pushboolean(L, 1);
	else
		lua_pushnil(L);
	return 1;
}

int lf_hash(struct lua_State *L)
{
	char hashstr[64];
	string_hash_tostr(string_hash(luaL_checklstring(L,1,NULL)), hashstr);
	lua_pushstring(L, hashstr);
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
			luaL_argerror(L, 1, lua_pushfstring(L, "unexpected %s in tables", luaL_typename(L, -1)));
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

	luaL_checknumarg_eq(L, 1);
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
	s = lua_rawlen(L, -1);
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
	luaL_checknumarg_eq(L, 1);
	luaL_checktype(L, 1, LUA_TTABLE);
	return table_deepcopy_r(L);
}

/* flattens a table into a simple table with strings */
static int lf_table_flatten_r(struct lua_State *L, int table_index, int flatten_index)
{
	size_t len;
	size_t cnt;

	/* Get the size of the table. */
	len = lua_rawlen(L, table_index);

	/* Loop over all numeric keys from 1 to the size of the table.
	 * Ignore all key-value pairs and numeric entries above the size of
	 * the table.
	 */
	cnt = 1;
	while( cnt<=len )
	{
		/* Push the next key on the stack. */
		lua_pushinteger(L, cnt);
		/* Get the table entry. */
		lua_gettable(L, table_index);
		/* Recurse if the entry is a table. */
		if( lua_istable(L, -1) )
		{
			flatten_index = lf_table_flatten_r(L, lua_gettop(L), flatten_index);
		}
		else if( lua_type(L, -1)==LUA_TSTRING )
		{
			lua_pushnumber(L, flatten_index); /* +3: key */
			lua_pushvalue(L, -2); /* +4: value */
			lua_settable(L, 2); /* pops +3 and +4 */

			flatten_index++;
		}
		else
		{
			/* other value */
			luaL_argerror(L, 1, lua_pushfstring(L, "unexpected %s in tables", luaL_typename(L, -1)));
		}

		/* pops +2 */
		lua_pop(L, 1);

		++cnt;
	}

	return flatten_index;
}

int lf_table_flatten(struct lua_State *L)
{
	size_t s;
	luaL_checknumarg_eq(L, 1);
	luaL_checktype(L, 1, LUA_TTABLE);

	/* 1: table to copy, 2: new table */
	s = lua_rawlen(L, -1);
	lua_createtable(L, 0, s);
	lf_table_flatten_r(L, 1, 1);
	return 1;
}

int lf_table_tostring(struct lua_State *L)
{
	/* 1: table 2: prefix, 3: postfix */
	static char string_buffer[1024*4];
	size_t prefix_len, postfix_len;
	size_t total_len = 0;
	size_t item_len = 0;
	size_t table_len = 0;
	size_t iterator = 0;	
	const char *prefix;
	const char *postfix;
	char *buffer;
	char *current;
	const char *item;
	
	luaL_checktype(L, 1, LUA_TTABLE);
	prefix = luaL_optlstring(L, 2, "", &prefix_len);
	postfix = luaL_optlstring(L, 3, "", &postfix_len);
	
	/* first, figure out the total size */
	table_len = lua_rawlen(L, 1);
	
	/* 4: iterator */
	for( iterator = 1; iterator <= table_len; iterator++ )
	{
		/* 5: value */
		lua_rawgeti(L, 1, iterator);

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
	for( iterator = 1; iterator <= table_len; iterator++ )
	{
		/* 5: value */
		lua_rawgeti(L, 1, iterator);

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



/* list directory functionallity */
typedef struct
{
	lua_State *lua;
	int i;
} LISTDIR_CALLBACK_INFO;

static void listdir_callback(const char *fullpath, const char *filename, int dir, void *user)
{
	LISTDIR_CALLBACK_INFO *info = (LISTDIR_CALLBACK_INFO *)user;
	lua_pushstring(info->lua, fullpath);
	lua_rawseti(info->lua, -2, info->i++);
}

int lf_listdir(lua_State *L)
{
	LISTDIR_CALLBACK_INFO info;
	info.lua = L;
	info.i = 1;
	
	/* create the table */
	lua_newtable(L);

	/* add all the entries */
	if(strlen(lua_tostring(L, 1)) < 1)
		file_listdirectory(context_get_path(L), listdir_callback, &info);
	else
	{
		char buffer[1024];
		path_join(context_get_path(L), -1, lua_tostring(L,1), -1, buffer, sizeof(buffer));
		file_listdirectory(buffer, listdir_callback, &info);
	}

	return 1;
}

/* collect functionallity */
enum
{
	COLLECTFLAG_FILES=1,
	COLLECTFLAG_DIRS=2,
	COLLECTFLAG_HIDDEN=4,
	COLLECTFLAG_RECURSIVE=8
};

typedef struct
{
	int path_len;
	const char *start_str;
	int start_len;
	
	const char *end_str;
	int end_len;
	
	lua_State *lua;
	int i;
	int flags;
} COLLECT_CALLBACK_INFO;

static void run_collect(COLLECT_CALLBACK_INFO *info, const char *input);

static void collect_callback(const char *fullpath, const char *filename, int dir, void *user)
{
	COLLECT_CALLBACK_INFO *info = (COLLECT_CALLBACK_INFO *)user;
	int filename_len = strlen(filename);

	/* don't process . and .. paths */
	if(filename[0] == '.')
	{
		if(filename[1] == 0)
			return;
		if(filename[1] == '.' && filename[2] == 0)
			return;
	}
	
	/* don't process hidden stuff if not wanted */
	if(filename[0] == '.' && !(info->flags&COLLECTFLAG_HIDDEN))
		return;

	do
	{
		/* check end */
		if(info->end_len > filename_len || strcmp(filename+filename_len-info->end_len, info->end_str))
			break;

		/* check start */
		if(info->start_len && strncmp(filename, info->start_str, info->start_len))
			break;
		
		/* check dir vs search param */
		if(!dir && info->flags&COLLECTFLAG_DIRS)
			break;
		
		if(dir && info->flags&COLLECTFLAG_FILES)
			break;
			
		/* all criterias met, push the result */
		lua_pushstring(info->lua, fullpath);
		lua_rawseti(info->lua, -2, info->i++);
	} while(0);
	
	/* recurse */
	if(dir && info->flags&COLLECTFLAG_RECURSIVE)
	{
		char recursepath[1024];
		COLLECT_CALLBACK_INFO recurseinfo = *info;
		strcpy(recursepath, fullpath);
		strcat(recursepath, "/");
		strcat(recursepath, info->start_str);
		run_collect(&recurseinfo, recursepath);
		info->i = recurseinfo.i;
	}	
}

static void run_collect(COLLECT_CALLBACK_INFO *info, const char *input)
{
	char dir[1024];
	int dirlen = 0;
	
	/* get the directory */
	path_directory(input, dir, sizeof(dir));
	dirlen = strlen(dir);
	info->path_len = dirlen+1;
	
	/* set the start string */
	if(dirlen)
		info->start_str = input + dirlen + 1;
	else
		info->start_str = input;
		
	for(info->start_len = 0; info->start_str[info->start_len]; info->start_len++)
	{
		if(info->start_str[info->start_len] == '*')
			break;
	}
	
	/* set the end string */
	if(info->start_str[info->start_len])
		info->end_str = info->start_str + info->start_len + 1;
	else
		info->end_str = info->start_str + info->start_len;
	info->end_len = strlen(info->end_str);
	
	/* search the path */
	file_listdirectory(dir, collect_callback, info);	
}

static int collect(lua_State *L, int flags)
{
	int n = lua_gettop(L);
	int i;
	COLLECT_CALLBACK_INFO info;

	/* create the table */
	lua_newtable(L);

	/* set common info */		
	info.lua = L;
	info.i = 1;
	info.flags = flags;

	/* start processing the input strings */
	for(i = 1; i <= n; i++)
	{
		const char *input = luaL_checklstring(L, i, NULL);
		
		if(!input)
			continue;
			
		run_collect(&info, input);
	}
	
	return 1;
}

int lf_collect(lua_State *L) { return collect(L, COLLECTFLAG_FILES); }
int lf_collectrecursive(lua_State *L) { return collect(L, COLLECTFLAG_FILES|COLLECTFLAG_RECURSIVE); }
int lf_collectdirs(lua_State *L) { return collect(L, COLLECTFLAG_DIRS); }
int lf_collectdirsrecursive(lua_State *L) { return collect(L, COLLECTFLAG_DIRS|COLLECTFLAG_RECURSIVE); }

/*  */
int lf_path_join(lua_State *L)
{
	char buffer[1024*2];
	int err;
	const char *base;
	const char *extend;
	size_t base_len, extend_len;

	luaL_checknumarg_eq(L, 2);	
	base = luaL_checklstring(L, 1, &base_len);
	extend = luaL_checklstring(L, 2, &extend_len);
	err = path_join(base, base_len, extend, extend_len, buffer, 2*1024);
	if(err != 0)
	{
		luaL_error(L, "path_join: error %d, couldn't join\n\t'%s'\n  and\n\t'%s'",
			err,
			lua_tostring(L, 1),
			lua_tostring(L, 2));
	}
	
	lua_pushstring(L, buffer);
	return 1;
}

/*  */
int lf_path_isnice(lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	lua_pushboolean(L, path_isnice(luaL_checklstring(L, 1, NULL)));
	return 1;
}

int lf_path_normalize(lua_State *L)
{
	const char *path;
	
	luaL_checknumarg_eq(L, 1);

	path = luaL_checklstring(L, 1, NULL);
	
	if(path_isnice(path))
	{
		/* path is ok */
		lua_pushstring(L, path);
	}
	else
	{
		/* normalize and return */
		char buffer[2*1024];
		strcpy(buffer, path);
		path_normalize(buffer);
		lua_pushstring(L, buffer);
	}
	
	return 1;
}

/*  */
int lf_path_ext(lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	lua_pushstring(L, path_ext(luaL_checklstring(L, 1, NULL)));
	return 1;
}


/*  */
int lf_path_base(lua_State *L)
{
	size_t org_len;
	size_t new_len;
	size_t count = 0;
	const char *cur = 0;
	const char *path = 0;

	luaL_checknumarg_eq(L, 1);
	
	path = luaL_checklstring(L, 1, &org_len);

	/* cut off the ext */
	new_len = org_len;
	for(cur = path; *cur; cur++, count++)
	{
		if(*cur == '.')
			new_len = count;
		else if(path_is_separator(*cur))
			new_len = org_len;
	}
		
	lua_pushlstring(L, path, new_len);
	return 1;
}


static int path_dir_length(const char *path)
{
	const char *cur = path;
	int total = 0;
	int len = -1;
	for(; *cur; cur++, total++)
	{
		if(path_is_separator(*cur))
			len = (int)(cur-path);
	}

	if(len == -1)
		return 0;
	return len;
}

/*  */
int lf_path_dir(lua_State *L)
{
	char buffer[1024];
	const char *path;

	luaL_checknumarg_eq(L, 1);

	path = luaL_checklstring(L, 1, NULL);
	
	/* check if we can take the easy way out */
	if(path_isnice(path))
	{
		lua_pushlstring(L, path, path_dir_length(path));
		return 1;
	}
	
	/* we must normalize the path as well */
	strncpy(buffer, path, sizeof(buffer) - 1);
	buffer[sizeof(buffer) - 1] = 0;
	path_normalize(buffer);
	lua_pushlstring(L, buffer, path_dir_length(buffer));
	return 1;
}

/*  */
int lf_path_filename(lua_State *L)
{
	luaL_checknumarg_eq(L, 1);
	lua_pushstring(L, path_filename(luaL_checklstring(L, 1, NULL)));
	return 1;
}

/* dependency functions */
static struct STRINGLIST *current_includepaths = NULL;
static hash_t current_includepaths_hash = 0;

/* */
int lf_add_dependency_cpp_set_paths(lua_State *L)
{
	struct CONTEXT *context;
	struct STRINGLIST * cur;
	int n = lua_gettop(L);
	
	if(n != 1)
		luaL_error(L, "add_dependency_cpp_set_paths: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TTABLE);
	
	context = context_get_pointer(L);
	current_includepaths = NULL;
	build_stringlist(L, context->deferredheap, &current_includepaths, 1);

	current_includepaths_hash = 0;
	for(cur = current_includepaths; cur; cur = cur->next)
		current_includepaths_hash = string_hash_add(current_includepaths_hash, cur->str);

	return 0;
}

/* */
extern int option_cdep2;
int lf_add_dependency_cpp(lua_State *L)
{
	struct CONTEXT *context;
	struct DEFERRED *deferred;
	struct NODE * node;
	struct DEFERRED_CSCAN *scan;
	int n = lua_gettop(L);
	int hashindex;
	
	if(n != 1)
		luaL_error(L, "add_dependency_cpp: incorrect number of arguments");
	luaL_checkstring(L,1);
	
	context = context_get_pointer(L);
	node = node_find(context->graph, lua_tostring(L,1));

	if(!node)
		luaL_error(L, "add_dependency_cpp: couldn't find node with name '%s'", lua_tostring(L,1));


	deferred = (struct DEFERRED *)mem_allocate(context->deferredheap, sizeof(struct DEFERRED));
	deferred->node = node;
	deferred->user = current_includepaths;
	deferred->depcontext = current_includepaths_hash;

	if(option_cdep2)
	{
		deferred->run = dep_cpp2;
		hashindex = deferred->depcontext&(CSCAN_HASHSIZE-1);

		for(scan = context->firstcscans[hashindex]; scan; scan = scan->next) {
			if(scan->includepaths_hash == current_includepaths_hash) {
				break;
			}
		}

		if(!scan)
		{
			scan = (struct DEFERRED_CSCAN *)mem_allocate(context->deferredheap, sizeof(struct DEFERRED_CSCAN));
			scan->includepaths_hash = current_includepaths_hash;
			scan->next = context->firstcscans[hashindex];
			context->firstcscans[hashindex] = scan;
		}

		deferred->next = scan->first;
		scan->first = deferred;
	} else {
		deferred->run = dep_cpp;
		deferred->next = context->firstdeferred_cpp;
		context->firstdeferred_cpp = deferred;
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
	deferred->run = dep_plain;
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

/* sleep for x seconds. Missing from lua standard api. */
int lf_sleep( lua_State* L ) {
	lua_Number luatime = 0.0;

	if ( lua_gettop( L ) != 1 )
		luaL_error( L, "sleep: expected 1 arguments" );

	luatime = luaL_checknumber( L, 1 );
	threads_sleep( (int)( luatime * 1e3 ) );

	return 0;
}