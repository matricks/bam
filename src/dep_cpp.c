#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LUA_CORE /* make sure that we don't try to import these functions */
#include <lua.h>
#include <lauxlib.h>

#include "path.h"
#include "node.h"
#include "cache.h"
#include "context.h"
#include "mem.h"
#include "support.h"
#include "session.h"
#include "luafuncs.h"

static int processline(char *line, char **start, char **end, int *systemheader)
{
	const char *include_text = "include";
	char *current = line;
	*start = 0;
	*end = 0;
	*systemheader = 0;
	
	/* search for # */
	while(*current != '#')
	{
		if(*current == ' ' || *current == '\t')
			current++; /* next char */
		else
			return 0; /* this catches \0 aswell */
	}
	
	current++; /* skip # */
	
	/* search for first character */
	while(1)
	{
		if(*current == ' ' || *current == '\t')
			current++;
		else if(*current == 0)
			return 0;
		else
			break;
	}
	
	/* match "include" */
	while(*include_text)
	{
		if(*current == *include_text)
		{
			current++;
			include_text++;
		}
		else
			return 0;
	}
	
	/* search for first character */
	while(1)
	{
		if(*current == ' ' || *current == '\t')
			current++;
		else if(*current == 0)
			return 0;
		else
			break;
	}

	/* match starting < or " */
	*start = current+1;
	if(*current == '<')
		*systemheader = 1;
	else if(*current == '"')
		*systemheader = 0;
	else
		return 0;
	
	/* skip < or " */
	current++;
	
	/* search for < or " to end it */
	while(1)
	{
		if(*current == '>' || *current == '"')
			break;
		else if(*current == 0)
			return 0;
		else
			current++;
	}
	
	*end = current; 
	return 1;
}

struct CACHERUNINFO
{
	struct CONTEXT *context;
	int (*callback)(struct NODE *, void *, const char *, int);
	void *userdata;
};

static int dependency_cpp_run(struct CONTEXT *context, struct NODE *node,
		int (*callback)(struct NODE *, void *, const char *, int), void *userdata);

static void cachehit_callback(struct NODE *node, struct CACHENODE *cachenode, void *user)
{
	struct CACHERUNINFO *info = (struct CACHERUNINFO *)user;
	
	/* check if the file has been removed */
	struct NODE *existing_node = node_find_byhash(node->graph, cachenode->hashid);
	if(existing_node)
	{
		struct NODE *newnode = node_add_dependency (node, existing_node);
		dependency_cpp_run(info->context, newnode, info->callback, info->userdata);
	}
	else
	{
		time_t timestamp = file_timestamp(cachenode->filename);
		if(timestamp)
		{
			/* this shouldn't be able to fail */
			struct NODE *newnode;
			node_create(&newnode, info->context->graph, cachenode->filename, NULL, timestamp);
			node_add_dependency (node, newnode);

			/* recurse the dependency checking */
			dependency_cpp_run(info->context, newnode, info->callback, info->userdata);
		}
		else
		{
			node->dirty = NODEDIRTY_MISSING;
		}
	}
}

/* dependency calculator for c/c++ preprocessor */
static int dependency_cpp_run(struct CONTEXT *context, struct NODE *node,
		int (*callback)(struct NODE *, void *, const char *, int), void *userdata)
{
	char *linestart;
	char *includestart;
	char *includeend;
	int systemheader;
	int errorcode = 0;
	int linecount = 0;

	/* open file */
	long filesize;
	long readitems;
	char *filebuf;
	char *filebufcur;
	char *filebufend;
	FILE *file;
	struct CACHERUNINFO cacheinfo;

	/* don't run depcheck twice */
	if(node->depchecked)
		return 0;
		
	/* mark the node for caching */
	node_cached(node);
	
	/* check if we have the dependencies in the cache frist */
	cacheinfo.context = context;
	cacheinfo.callback = callback;
	cacheinfo.userdata = userdata;
	if(cache_do_dependency(context, node, cachehit_callback, &cacheinfo))
		return 0;

	/* mark the node as checked */
	node->depchecked = 1;

	file = fopen(node->filename, "rb");
	if(!file)
		return 0;
	
	/* read the whole file */
	fseek(file, 0, SEEK_END);
	filesize = ftell(file);
	fseek(file, 0, SEEK_SET);
	
	filebuf = malloc(filesize+1); /* +1 for null termination */
	
	if(!filebuf)
	{
		printf("cpp-dep: %s: error allocating %ld bytes\n", node->filename, filesize);
		fclose(file);
		return 1;
	}
		
	/* read the file and close it */
	readitems = fread(filebuf, 1, filesize, file);
	fclose(file);

	if(readitems != filesize)
	{
		printf("cpp-dep: %s: error reading. %ld of %ld bytes read\n", node->filename, readitems, filesize);
		free(filebuf);
		return 1;
	}
	
	filebufcur = filebuf;
	filebufend = filebuf+filesize;
	
	while(filebufcur < filebufend)
	{
		/* search for next line */
		linestart = filebufcur;
		while(filebufcur != filebufend && *filebufcur != '\n' && *filebufcur != '\r')
			filebufcur++;
		*filebufcur = 0;
		filebufcur++;
		linecount++;

		if(processline(linestart, &includestart, &includeend, &systemheader))
		{
			*includeend = 0;
			/* run callback */
			errorcode = callback(node, userdata, includestart, systemheader);
			if(errorcode)
				break;
		}
	}

	/* clean up and return*/
	free(filebuf);
	return errorcode;
}

struct CPPDEPINFO
{
	struct CONTEXT *context;
	struct STRINGLIST *paths;
};

static int node_findfile(struct GRAPH *graph, const char *filename, struct NODE **node, time_t *timestamp)
{
	/* first check the graph */
	*node = node_find(graph, filename);
	if(*node)
		return 1;

	/* then check the file system */
	*timestamp = file_timestamp(filename);
	if(*timestamp)
		return 1;

	return 0;
}

/* */
static int dependency_cpp_callback(struct NODE *node, void *user, const char *filename, int sys)
{
	struct CPPDEPINFO *depinfo = (struct CPPDEPINFO *)user;
	struct CPPDEPINFO recurseinfo;
	char buf[MAX_PATH_LENGTH];
	int check_system = sys;

	int found = 0;
	struct NODE *depnode = NULL;
	time_t timestamp = 0;
	
	
	if(!sys)
	{
		/* "normal.header" */
		int flen = strlen(node->filename)-1;
		while(flen)
		{
			if(node->filename[flen] == '/')
				break;
			flen--;
		}
		path_join(node->filename, flen, filename, -1, buf, sizeof(buf));
		
		if(node_findfile(node->graph, buf, &depnode, &timestamp))
			found = 1;
		else
		{
			/* file does not exist */
			check_system = 1;
		}
	}

	if(check_system)
	{
		/* <system.header> */
		if(path_isabs(filename))
		{
			if(node_findfile(node->graph, filename, &depnode, &timestamp))
			{
				strcpy(buf, filename);
				found = 1;
			}
		}
		else
		{
			struct STRINGLIST *cur;
			int flen = strlen(filename);

			for(cur = depinfo->paths; cur; cur = cur->next)
			{
				path_join(cur->str, cur->len, filename, flen, buf, sizeof(buf));
				if(node_findfile(node->graph, buf, &depnode, &timestamp))
				{
					found = 1;
					break;
				}
			}
		}
	}

	/* */
	if(found)
	{
		path_normalize(buf);
		if(!depnode)
			node_create(&depnode, node->graph, buf, NULL, timestamp);
		if(node_add_dependency (node, depnode) == NULL)
			return 2;

		if(!depnode)
			return 3;
	
		/* do the dependency walk */
		if(!depnode->depchecked)
		{
			recurseinfo.paths = depinfo->paths;
			recurseinfo.context = depinfo->context;
			if(dependency_cpp_run(depinfo->context, depnode, dependency_cpp_callback, &recurseinfo) != 0)
				return 4;
		}
	}
		
	return 0;
}

static struct STRINGLIST *current_includepaths = NULL;

static int dependency_cpp_do_run(struct CONTEXT *context, struct DEFERRED *info)
{
	struct CPPDEPINFO depinfo;
	depinfo.context = context;
	depinfo.paths = (struct STRINGLIST *)info->user;
	if(dependency_cpp_run(context, info->node, dependency_cpp_callback, &depinfo) != 0)
		return -1;
	return 0;
}

/* */
int lf_add_dependency_cpp_set_paths(lua_State *L)
{
	struct CONTEXT *context;
	int n = lua_gettop(L);
	
	if(n != 1)
		luaL_error(L, "add_dependency_cpp_set_paths: incorrect number of arguments");
	luaL_checktype(L, 1, LUA_TTABLE);
	
	context = context_get_pointer(L);
	current_includepaths = NULL;
	build_stringlist(L, context->deferredheap, &current_includepaths, 1);
	return 0;
}

/* */
int lf_add_dependency_cpp(lua_State *L)
{
	struct CONTEXT *context;
	struct DEFERRED *deferred;
	int n = lua_gettop(L);
	
	if(n != 1)
		luaL_error(L, "add_dependency_cpp_set: incorrect number of arguments");
	luaL_checkstring(L,1);
	
	context = context_get_pointer(L);
	
	deferred = (struct DEFERRED *)mem_allocate(context->deferredheap, sizeof(struct DEFERRED));
	deferred->node = node_find(context->graph, lua_tostring(L,1));
	deferred->user = current_includepaths;
	deferred->run = dependency_cpp_do_run;
	deferred->next = context->firstdeferred_cpp;
	context->firstdeferred_cpp = deferred;
	return 0;
}
