#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "path.h"
#include "node.h"
#include "cache.h"
#include "statcache.h"
#include "context.h"
#include "mem.h"
#include "support.h"
#include "session.h"

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


/*
	scans a file for headers and adds them to the nodes c header references
*/
static int scan_source_file(struct CONTEXT * context, struct NODE *node)
{
	char *linestart;
	char *includestart;
	char *includeend;
	int systemheader;
	int linecount = 0;

	/* open file */
	long filesize;
	long readitems;
	char *filebuf;
	char *filebufcur;
	char *filebufend;
	FILE *file;
	struct CHEADERREF * currentref = NULL;
	
	if(node->headerscanned)
		return 0;

	/* mark that we have header scanned this one */
	node->headerscanned = 1;

	/* check the cache first */
	node->firstcheaderref = scancache_find(context->scancache, node);
	if ( node->firstcheaderref ) {
		return 0;
	}

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

		/* process the line */
		if(processline(linestart, &includestart, &includeend, &systemheader))
		{
			struct CHEADERREF * header = (struct CHEADERREF *)mem_allocate(node->graph->heap, sizeof(struct CHEADERREF));
			*includeend = 0;
			header->filename_len = includeend - includestart + 1;
			header->filename = string_duplicate(node->graph->heap, includestart, includeend - includestart);

			if ( currentref ) {
				currentref->next = header;
			} else {
				node->firstcheaderref = header;
			}
			currentref = header;
		}
	}

	/* clean up and return */
	free(filebuf);
	return 0;
}

struct CACHERUNINFO
{
	struct CONTEXT *context;
	int (*callback)(struct NODE *, void *, const char *, int);
	void *userdata;
};

static int dependency_cpp_run(struct CONTEXT *context, struct NODE *node,
		int (*callback)(struct NODE *, void *, const char *, int), void *userdata)
{
	scan_source_file(context, node);
	int errorcode = 0;

	struct CHEADERREF * curref = node->firstcheaderref;
	for(; curref; curref = curref->next)
	{
		errorcode = callback(node, userdata, curref->filename, curref->sys);
		if(errorcode)
			return errorcode;
	}
	return 0;
}

struct CPPDEPINFO
{
	struct CONTEXT *context;
	struct STRINGLIST *paths;
	hash_t depcontext;
};

static int node_findfile(struct GRAPH *graph, struct STATCACHE* statcache, const char *filename, struct NODE **node, time_t *timestamp)
{
	/* first check the graph */
	*node = node_find(graph, filename);
	if(*node)
		return 1;

	int isregular = 0;
	if(statcache_getstat( statcache, filename, timestamp, &isregular)==0)
		return *timestamp != 0 && isregular == 1;

	/* then check the file system */
	*timestamp = file_timestamp(filename);
	if(*timestamp && file_isregular(filename))
		return 1;

	return 0;
}

/* */
static int dependency_cpp_callback(struct NODE *node, void *user, const char *filename, int sys)
{
	struct CPPDEPINFO *depinfo = (struct CPPDEPINFO *)user;
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
		
		if(node_findfile(node->graph, depinfo->context->statcache, buf, &depnode, &timestamp))
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
			if(node_findfile(node->graph, depinfo->context->statcache, filename, &depnode, &timestamp))
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
				if(node_findfile(node->graph, depinfo->context->statcache, buf, &depnode, &timestamp))
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
		if(node_add_dependency(node, depnode) == NULL)
			return 2;

		if(!depnode)
			return 3;
	
		/* do the dependency walk */
		if(depnode->depcontext != depinfo->depcontext)
		{
			depnode->depcontext = depinfo->depcontext;
			if(dependency_cpp_run(depinfo->context, depnode, dependency_cpp_callback, depinfo) != 0)
				return 4;
		}
	}
		
	return 0;
}

int dep_cpp2(struct CONTEXT *context, struct DEFERRED *info)
{
	struct CPPDEPINFO depinfo;
	depinfo.context = context;
	depinfo.paths = (struct STRINGLIST *)info->user;
	depinfo.depcontext = info->depcontext;
	
	if(info->node->depcontext == info->depcontext)
		return 0;

	if(dependency_cpp_run(context, info->node, dependency_cpp_callback, &depinfo) != 0)
		return -1;
	return 0;
}
