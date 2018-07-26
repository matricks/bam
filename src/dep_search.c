#include "path.h"
#include "context.h"
#include "mem.h"
#include "node.h"
#include "support.h"
#include "dep.h"

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
int dep_plain(struct CONTEXT *context, struct DEFERRED *info)
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
