#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

#include "path.h"
#include "node.h"
#include "cache.h"
#include "context.h"
#include "mem.h"
#include "support.h"

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
	int (*callback)(void *, const char *, int);
	void *userdata;
};

static int dependency_cpp_run(struct CONTEXT *context, struct NODE *node,
		int (*callback)(void *, const char *, int), void *userdata);

static void cachehit_callback(struct NODE *node, void *user)
{
	struct CACHERUNINFO *info = (struct CACHERUNINFO *)user;
	dependency_cpp_run(info->context, node, info->callback, info->userdata);
}

/* dependency calculator for c/c++ preprocessor */
static int dependency_cpp_run(struct CONTEXT *context, struct NODE *node,
		int (*callback)(void *, const char *, int), void *userdata)
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
		printf("cpp-dep: error allocating %ld bytes\n", filesize);
		fclose(file);
		return 1;
	}
		
	/* read the file and close it */
	readitems = fread(filebuf, 1, filesize, file);
	fclose(file);

	if(readitems != filesize)
	{
		printf("cpp-dep: error reading the complete file. %ld of %ld bytes read\n", readitems, filesize);
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
			errorcode = callback(userdata, includestart, systemheader);
			if(errorcode)
			{
				printf("cpp-dep: error %d during callback\n", errorcode);
				break;
			}
		}
	}

	/* clean up and return*/
	free(filebuf);
	return errorcode;
}

struct CPPDEPPATH
{
	char *path;
	struct CPPDEPPATH *next;
};

struct CPPDEPINFO
{
	struct CONTEXT *context;
	struct NODE *node;
	struct CPPDEPPATH *first_path;
};

struct SCANNER_CPP
{
	struct SCANNER scanner;
	struct CPPDEPPATH *first_path;
};

/* */
static int dependency_cpp_callback(void *user, const char *filename, int sys)
{
	struct CPPDEPINFO *depinfo = (struct CPPDEPINFO *)user;
	struct NODE *node = depinfo->node;
	struct NODE *depnode;
	struct CPPDEPINFO recurseinfo;
	char buf[512];
	char normal[512];
	int check_system = sys;
	int found = 0;
	
	if(!sys)
	{
		/* "normal.header" */
		int flen = strlen(node->filename)-1; 
		int flen2 = strlen(filename);
		while(flen)
		{
			if(node->filename[flen] == '/')
				break;
			flen--;
		}

		if(flen == 0)
			memcpy(buf, filename, flen2+1);
		else
		{
			memcpy(buf, node->filename, flen+1);
			memcpy(buf+flen+1, filename, flen2);
			buf[flen+flen2+1] = 0;
		}
		
		if(!file_exist(buf) && !node_find(node->graph, filename))
		{
			/* file does not exist */
			memcpy(normal, buf, 512);
			check_system = 1;
		}
		else
			found = 1;
	}

	if(check_system)
	{
		/* <system.header> */
		if(path_isabs(filename))
		{
			if(file_exist(filename) || node_find(node->graph, filename))
			{
				strcpy(buf, filename);
				found = 1;
			}
		}
		else
		{
			struct CPPDEPPATH *cur;
			int flen = strlen(filename);
			int plen;

			for(cur = depinfo->first_path; cur; cur = cur->next)
			{
				plen = strlen(cur->path);
				memcpy(buf, cur->path, plen);
				memcpy(buf+plen, filename, flen+1); /* copy the 0-term aswell */
				
				if(file_exist(buf) || node_find(node->graph, buf))
				{
					found = 1;
					break;
				}
			}
		}
	
		/* no header found */
		if(!found)
		{
			if(sys)
				return 0;
			else
				memcpy(buf, normal, 512);
		}
	}
	
	/* */
	path_normalize(buf);
	depnode = node_add_dependency(node, buf);
	if(!depnode)
		return 2;
	
	/* do the dependency walk */
	if(found && !depnode->depchecked)
	{
		recurseinfo.first_path = depinfo->first_path;
		recurseinfo.node = depnode;
		recurseinfo.context = depinfo->context;
		if(dependency_cpp_run(depinfo->context, depnode, dependency_cpp_callback, &recurseinfo) != 0)
			return 3;
	}
	
	return 0;
}

/* */
int lf_dependency_cpp(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	struct HEAP *includeheap;
	struct CPPDEPINFO depinfo;
	struct CPPDEPPATH *cur_path;
	struct CPPDEPPATH *prev_path;
	int n = lua_gettop(L);
	const char *string;
	size_t string_length;
	
	if(n != 2)
		luaL_error(L, "dependency_cpp: incorrect number of arguments");

	if(!lua_isstring(L,1))
		luaL_error(L, "dependency_cpp: expected string");

	if(!lua_istable(L,2))
		luaL_error(L, "dependency_cpp: expected table");
	
	/* fetch context */
	context = context_get_pointer(L);
	node = node_find(context->graph, lua_tostring(L,1));
	
	/* */
	if(!node)
		luaL_error(L, "dependency_cpp: node '%s' not found", lua_tostring(L,1));
	
	/* create a heap to store the includes paths in */
	includeheap = mem_create();

	/* fetch the system include paths */
	lua_pushnil(L);
	cur_path = 0x0;
	prev_path = 0x0;
	depinfo.first_path = 0x0;
	while(lua_next(L, 2))
	{
		if(lua_type(L,-1) == LUA_TSTRING)
		{
			/* allocate the path */
			string = lua_tolstring(L, -1, &string_length);
			cur_path = (struct CPPDEPPATH*)mem_allocate(includeheap, sizeof(struct CPPDEPPATH)+string_length+2);
			cur_path->path = (char *)(cur_path+1);
			cur_path->next = 0x0;
		
			/* copy path and terminate with a / */
			memcpy(cur_path->path, string, string_length);
			cur_path->path[string_length] = '/';
			cur_path->path[string_length+1] = 0;
		
			/* add it to the chain */
			if(prev_path)
				prev_path->next = cur_path;
			else
				depinfo.first_path = cur_path;
			prev_path = cur_path;
		}
		
		/* pop the value, keep the key */
		lua_pop(L, 1);
	}
	
	/* do the dependency walk */
	depinfo.context = context;
	depinfo.node = node;
	if(dependency_cpp_run(context, node, dependency_cpp_callback, &depinfo) != 0)
	{
		mem_destroy(includeheap);
		luaL_error(L, "dependency_cpp: error during depencency check");
	}

	/* free the include heap */
	mem_destroy(includeheap);
	return 0;
}
