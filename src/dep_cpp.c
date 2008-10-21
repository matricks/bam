#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>

#include "path.h"
#include "node.h"
#include "context.h"
#include "mem.h"
#include "support.h"

/* this should perhaps be removed */
static int option_cppdep_includewarnings = 0;

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

/* dependency calculator for c/c++ preprocessor */
static int dependency_cpp_run(const char *filename, 
		int (*callback)(void *, const char *, int), void *userdata)
{
	const int debug = 0;
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

	if(debug)
		printf("cpp-dep: running on %s\n", filename);

	file = fopen(filename, "rb");
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
			if(debug) printf("INCLUDE: %s\n", includestart);
			
			/* run callback */
			errorcode = callback(userdata, includestart, systemheader);
			if(errorcode)
			{
				printf("cpp-dep: error %d during callback\n", errorcode);
				break;
			}
		}
	}

	if(debug)
		printf("cpp-dep: %s=%d lines\n", filename, linecount);
	
	/* clean up */
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
	int extra_debug = 0;
	
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
		
		memcpy(buf, node->filename, flen+1);
		memcpy(buf+flen+1, filename, flen2);
		buf[flen+flen2+1] = 0;
		
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
				
				if(extra_debug)
					printf("\tsearching for %s\n", buf);

				if(file_exist(buf) || node_find(node->graph, buf))
				{
					if(option_cppdep_includewarnings && !sys)
					{
						printf("dependency_cpp: c++ dependency error. could not find \"%s\" as a relative file but as a system header.\n", filename);
						return -1;
					}

					found = 1;

					/* printf("found system header header %s at %s\n", filename, buf); */
					break;
				}
			}
		}
	
		/* no header found */
		if(!found)
		{
			/* if it was a <system.header> we are just gonna ignore it and hope that it never */
			/* changes */
			if(!sys && option_cppdep_includewarnings)
			{
				/* not a system header so we gonna bitch about it */
				char a[] = {'"', '<'};
				char b[] = {'"', '>'};
				printf("dependency_cpp: C++ DEPENDENCY WARNING: header not found. %c%s%c\n", a[sys], filename, b[sys]);
			}

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
		depnode->depchecked = 1;
		if(dependency_cpp_run(buf, dependency_cpp_callback, &recurseinfo) != 0)
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
	int i;
	
	if(n != 2)
	{
		lua_pushstring(L, "dependency_cpp: incorrect number of arguments");
		lua_error(L);
	}

	if(!lua_isstring(L,1))
	{
		lua_pushstring(L, "dependency_cpp: expected string");
		lua_error(L);
	}

	if(!lua_istable(L,2))
	{
		lua_pushstring(L, "dependency_cpp: expected table");
		lua_error(L);
	}
	
	/* no need to do the depenceny stuff if we only going to clean */
	/* TODO: reintroduce */
	/* if(option_clean)
		return 0; */
	
	/* fetch context */
	context = context_get_pointer(L);
	
	/* create a heap to store the includes paths in */
	includeheap = mem_create();

	/* fetch the system include paths */
	lua_pushnil(L);
	cur_path = 0x0;
	prev_path = 0x0;
	depinfo.first_path = 0x0;
	while(lua_next(L, 2))
	{
		if(lua_isstring(L,-1))
		{
			/* allocate the path */
			i = strlen(lua_tostring(L,-1));
			cur_path = (struct CPPDEPPATH*)mem_allocate(includeheap, sizeof(struct CPPDEPPATH)+i+2);
			cur_path->path = (char *)(cur_path+1);
			cur_path->next = 0x0;
		
			/* copy path and terminate with a / */
			memcpy(cur_path->path, lua_tostring(L,-1), i);
			cur_path->path[i] = '/';
			cur_path->path[i+1] = 0;
		
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

	/*for(cur_path = depinfo.first_path; cur_path; cur_path = cur_path->next)
		printf("p: %s\n", cur_path->path);*/
	
	/* */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
	{
		mem_destroy(includeheap);
		lua_pushstring(L, "dependency_cpp: node not found");
		lua_error(L);
	}
	
	/* do the dependency walk */
	/* TODO: caching system */
	depinfo.node = node;
	depinfo.node->depchecked = 1;
	if(dependency_cpp_run(node->filename, dependency_cpp_callback, &depinfo) != 0)
	{
		mem_destroy(includeheap);
		lua_pushstring(L, "dependency_cpp: error during depencency check");
		lua_error(L);
	}

	/* free the include heap */
	mem_destroy(includeheap);
	
	return 0;
}
