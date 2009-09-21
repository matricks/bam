#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h> /* system() */

#include "mem.h"
#include "context.h"
#include "node.h"
#include "cache.h"
#include "support.h"
#include "session.h"

const char *CONTEXT_LUA_SCRIPTARGS_TABLE = "_bam_scriptargs";
const char *CONTEXT_LUA_TARGETS_TABLE = "_bam_targets";
const char *CONTEXT_LUA_PATH = "_bam_path";
const char *CONTEXT_LUA_WORKPATH = "_bam_workpath";

/* */
struct CONTEXT *context_get_pointer(lua_State *L)
{
	/* HACK: we store the context pointer as the user data to
		to the allocator for fast access to it */
	void *context;
	lua_getallocf(L, &context);
	return (struct CONTEXT*)context;
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

int context_default_target(struct CONTEXT *context, struct NODE *node)
{
	context->defaulttarget = node;
	return 0;
}

static void progressbar_clear()
{
	printf("                                                 \r");
}

static void progressbar_draw(struct CONTEXT *context)
{
	const int max = 40;
	int i;
	int count = (context->current_cmd_num*max)/context->num_commands;
	int precent = (context->current_cmd_num*100)/context->num_commands;

	if(session.report_color)
	{
		printf(" %3d%% \033[01;32m[\033[01;33m", precent);
		for(i = 0; i < count-1; i++)
			printf("=");
		printf(">");
		for(; i < max; i++)
			printf(" ");
		printf("\033[01;32m]\033[00m\r");
	}
	else
	{
		printf(" %3d%% [", precent);
		for(i = 0; i < count-1; i++)
			printf("=");
		printf(">");
		for(; i < max; i++)
			printf(" ");
		printf("]\r");
	}
}

static int run_node(struct CONTEXT *context, struct NODE *node, int thread_id)
{
	static const char *format = 0;
	int ret;
	
	if(node->label && node->label[0])
	{
		context->current_cmd_num++;
	
		if(!format)
		{
			static char buf[64];
			int num = 0;
			int c = context->num_commands;
			for(; c; c /= 10)
				num++;
			
			if(session.report_color)
				sprintf(buf, "\033[01;32m[%%%dd/%%%dd] \033[01;36m#%%d\033[00m %%s\n", num, num);
			else
				sprintf(buf, "[%%%dd/%%%dd] #%%d %%s\n", num, num);
			format = buf;
		}
		
		if(session.report_bar)
			progressbar_clear();
		if(session.report_steps)
		{
			if(session.simpleoutput)
				printf("%s", node->label);
			else
				printf(format, context->current_cmd_num, context->num_commands, thread_id, node->label);
		}
		
		if(session.report_bar)
			progressbar_draw(context);
	}
	
	if(session.verbose)
	{
		if(session.report_color)
			printf("\033[01;33m%s\033[00m\n", node->cmdline);
		else
			printf("%s\n", node->cmdline);
	}
		
	fflush(stdout);
	
	/* execute the command */
	criticalsection_leave();
	ret = run_command(node->cmdline);
	criticalsection_enter();
	
	if(ret)
	{
		if(session.report_color)
			printf("\033[01;31m%s: command returned error %d\033[00m\n", session.name, ret);
		else
			printf("%s: command returned error: %d\n", session.name, ret);
		fflush(stdout);
	}
	return ret;
}

struct THREADINFO
{
	int id;
	struct CONTEXT *context;
	int errorcode;
};

static int threads_run_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct THREADINFO *info = (struct THREADINFO *)walkinfo->user;
	struct DEPENDENCY *dep;
	int errorcode = 0;
	
	/* check global error code so we know if we should exit */
	if(info->context->exit_on_error && info->context->errorcode)
		return info->context->errorcode;

	/* make sure that all deps are done and propagate broken status */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(dep->node->workstatus == NODESTATUS_BROKEN)
		{
			node->workstatus = NODESTATUS_BROKEN;
			return info->context->errorcode;
		}
			
		if(dep->node->dirty && dep->node->workstatus != NODESTATUS_DONE)
			return 0;
	}

	/* if it doesn't have a tool, just mark it as done and continue the search */
	if(!node->cmdline)
	{
		node->workstatus = NODESTATUS_DONE;
		return 0;
	}
	
	/* mark the node as its in the working */
	node->workstatus = NODESTATUS_WORKING;
	
	/* run the node */
	if(node->cmdline)
		errorcode = run_node(info->context, node, info->id+1);
	
	/* this node is done, mark it so and return the error code */
	if(errorcode)
	{
		node->workstatus = NODESTATUS_BROKEN;
		
		/* set global error code */
		info->context->errorcode = errorcode;
	}
	else
		node->workstatus = NODESTATUS_DONE;
	return errorcode;
}

/* signal handler */
static volatile int abortrequest = 0;
static void abortsignal(int i)
{
	(void)i;
	printf("%s: signal cought, waiting for commands to finish.\n", session.name);
	abortrequest = 1;
}

static void threads_run(void *u)
{
	struct THREADINFO *info = (struct THREADINFO *)u;
	struct NODE *target = info->context->target;
	int flags = NODEWALK_BOTTOMUP|NODEWALK_UNDONE|NODEWALK_QUICK;
	
	info->errorcode = 0;
	
	/* lock the dependency graph */
	criticalsection_enter();
	
	install_signals(abortsignal);
	
	if(target->dirty)
	{
		while(1)
		{
			info->errorcode = node_walk(target, flags, threads_run_callback, info);
			
			if(abortrequest)
				break;

			/* check if we are done */
			if(target->workstatus != NODESTATUS_UNDONE)
				break;

			if(info->context->exit_on_error && info->context->errorcode)
				break;
			
			/* let the others have some time */
			criticalsection_leave();
			threads_yield();
			criticalsection_enter();
		}
	}
	
	criticalsection_leave();
}

int context_build_make(struct CONTEXT *context)
{
	/* multithreaded */
	struct THREADINFO info[64];
	void *threads[64];
	int i;
	
	/* clamp number of threads */
	if(session.threads > 64) session.threads = 64;
	else if(session.threads < 1) session.threads = 1;
	
	for(i = 0; i < session.threads; i++)
	{
		info[i].context = context;
		info[i].id = i;
		info[i].errorcode = 0;
	}

	if(session.threads <= 1)
	{
		/* no threading, use this thread then */
		threads_run(&info[0]);
		if(session.report_bar)
			progressbar_clear();
		return info[0].errorcode;
	}
	else
	{
		/* start threads */
		for(i = 0; i < session.threads; i++)
			threads[i] = threads_create(threads_run, &info[i]);
		
		/* wait for threads */
		for(i = 0; i < session.threads; i++)
			threads_join(threads[i]);
			
		if(session.report_bar)
			progressbar_clear();

		/* check for error codes */		
		for(i = 0; i < session.threads; i++)
		{
			if(info[i].errorcode)
				return info[i].errorcode;
		}
	}
	return 0;
}

static int build_clean_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	
	/* no tool, no processing */
	if(!node->cmdline)
		return 0;

	if(node->timestamp)
	{
		if(remove(node->filename) == 0)
			printf("%s: removed '%s'\n", session.name, node->filename);
	}
	return 0;
}

int context_build_clean(struct CONTEXT *context)
{
	return node_walk(context->target, NODEWALK_BOTTOMUP|NODEWALK_FORCE, build_clean_callback, 0);
}

static int build_prepare_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct CONTEXT *context = (struct CONTEXT *)walkinfo->user;
	struct CACHENODE *cachenode;
	struct DEPENDENCY *dep;
	struct NODEWALKPATH *path;
	
	time_t time = timestamp();
	
	if(node->depth < walkinfo->depth)
		node->depth = walkinfo->depth;
	
	if(node->cmdline)
	{
		/* dirty checking, check against cmdhash and global timestamp first */
		if(!node->timestamp)
			node->dirty = 1;
		else
		{
			cachenode = cache_find_byhash(context->cache, node->hashid);
			if(cachenode)
			{
				if(cachenode->cmdhash != node->cmdhash)
					node->dirty = NODEDIRTY_CMDHASH;
			}
			else if(node->timestamp < context->globaltimestamp)
				node->dirty = NODEDIRTY_GLOBALSTAMP;
		}
	}
	
	/* check against all the dependencies */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		/* time sanity check */
		if(node->timestamp > time)
			printf("%s: WARNING:'%s' comes from the future\n", session.name, node->filename);

		/* do circular action dependency checking */
		if(dep->node->cmdline)
		{
			for(path = walkinfo->parent; path; path = path->parent)
			{
				if(path->node == dep->node)
				{
					printf("error: circular dependency found\n");
					printf("\t%s\n", dep->node->filename);
					for(path = walkinfo->parent; path; path = path->parent)
						printf("\t%s\n", path->node->filename);
					return -1;
				}
			}
		}

		/* update dirty */		
		if(!node->dirty)
		{
			if(dep->node->dirty)
				node->dirty = NODEDIRTY_DEPDIRTY;
			else if(node->timestamp < dep->node->timestamp)
			{
				if(node->cmdline)
					node->dirty = NODEDIRTY_DEPNEWER;
				else /* no cmdline, just propagate the timestamp */
					node->timestamp = dep->node->timestamp;
			}
		}
	}

	/* count commands */
	if(node->cmdline && node->dirty && !node->counted)
	{
		node->counted = 1;
		context->num_commands++;
	}

	return 0;
}

/* prepare does time sanity checking, dirty propagation,
	graph validation and job counting */
int context_build_prepare(struct CONTEXT *context)
{
	/* TODO: we should have NODEWALK_QUICK here, but then it will fail on circular dependencies */
	return node_walk(context->target, NODEWALK_BOTTOMUP|NODEWALK_FORCE, build_prepare_callback, context);
}
