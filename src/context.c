#define LUA_CORE /* make sure that we don't try to import these functions */
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h> /* system() */

#include "mem.h"
#include "context.h"
#include "path.h"
#include "node.h"
#include "cache.h"
#include "support.h"
#include "session.h"

#ifndef BAM_MAX_THREADS
        #define BAM_MAX_THREADS 1024
#endif

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
	int count = (context->current_job_num*max)/context->num_jobs;
	int precent = (context->current_job_num*100)/context->num_jobs;

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

static void constraints_update(struct JOB *job, int direction)
{
	struct NODELINK *link;
	for(link = job->constraint_shared; link; link = link->next)
		link->node->job->constraint_shared_count += direction;
	for(link = job->constraint_exclusive; link; link = link->next)
		link->node->job->constraint_exclusive_count += direction;
}

/* returns 0 if there are no constraints that are conflicting */
static int constraints_check(struct JOB *job)
{
	struct NODELINK *link;
	for(link = job->constraint_shared; link; link = link->next)
	{
		if(link->node->job->constraint_exclusive_count)
			return 1;
	}
	
	for(link = job->constraint_exclusive; link; link = link->next)
	{
		if(link->node->job->constraint_exclusive_count || link->node->job->constraint_shared_count)
			return 1;
	}
	
	return 0;
}

/* prints infomation about the job being run */
static void runjob_print_report(struct CONTEXT *context, struct JOB *job, int thread_id)
{
	static const char *format = 0;

	if(!format)
	{
		static char buf[64];
		int num = 0;
		int c = context->num_jobs;
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
			printf("%s", job->label);
		else
			printf(format, context->current_job_num, context->num_jobs, thread_id, job->label);
	}
	
	if(session.report_bar)
		progressbar_draw(context);
	
	if(session.verbose)
	{
		if(session.report_color)
			printf("\033[01;33m%s\033[00m\n", job->cmdline);
		else
			printf("%s\n", job->cmdline);
	}
		
	fflush(stdout);
}

static int runjob_create_outputpaths(struct JOB *job)
{
	struct NODELINK *link;

	/* create output path */
	for(link = job->firstoutput; link; link = link->next)
	{
		/* TODO: perhaps we can skip running this if we know that the file exists on disk already */
		if(file_createpath(link->node->filename) != 0)
		{
			if(session.report_color)
				printf("\033[01;31m");
			
			printf("%s: could not create output directory for '%s'\n", session.name, link->node->filename);

			if(session.report_color)
				printf("\033[00m");
				
			fflush(stdout);
			return 1;
		}
	}

	return 0;
}

static int run_job(struct CONTEXT *context, struct JOB *job, int thread_id)
{
	struct NODELINK *link;
	int errorcode;

	context->current_job_num++;

	/* mark the node as its in the working */
	job->status = JOBSTATUS_WORKING;

	/* print some nice information */
	runjob_print_report(context, job, thread_id);

	/* create output paths */
	if(runjob_create_outputpaths(job) != 0)
	{
		job->status = JOBSTATUS_BROKEN;
		context->errorcode = 1;
		return 1;
	}

	/* add constraints count */
	constraints_update(job, 1);
	
	event_begin(thread_id, "job", job->label);

	/* execute the command */
	criticalsection_leave();
	errorcode = run_command(job->cmdline, job->filter);
	if(errorcode == 0)
	{
		/* make sure that the tool updated the timestamp */
		for(link = job->firstoutput; link; link = link->next)
			file_touch(link->node->filename);
	}
	criticalsection_enter();

	event_end(thread_id, "job", NULL);
	
	/* sub constraints count */
	constraints_update(job, -1);
	
	if(errorcode == 0)
	{
		/* job done successfully */
		job->status = JOBSTATUS_DONE;
		job->cachehash = job->cmdhash;
	}
	else
	{
		/* set global error code */
		job->status = JOBSTATUS_BROKEN;
		context->errorcode = errorcode;

		/* report the error */
		if(session.report_color)
			printf("\033[01;31m");
		
		printf("%s: '%s' error %d\n", session.name, job->label, errorcode);
		
		for(link = job->firstoutput; link; link = link->next)
		{
			if(file_timestamp(link->node->filename) != link->node->timestamp_raw)
			{
				remove(link->node->filename);
				printf("%s: '%s' removed because job updated it even it failed.\n", session.name, link->node->filename);
			}
		}

		if(session.report_color)
			printf("\033[00m");
			
		fflush(stdout);
	}
	return errorcode;
}

struct THREADINFO
{
	int id;
	struct CONTEXT *context;
};

/* returns 1 if we can run this job */
static int check_job(struct CONTEXT *context, struct JOB *job)
{
	struct NODELINK *link;
	int broken = 0;

	if(job->status != JOBSTATUS_UNDONE)
		return 0;

	/* make sure that all deps are done and propagate broken status */
	for(link = job->firstjobdep; link; link = link->next)
	{
		if(link->node->job->status == JOBSTATUS_BROKEN)
			broken = 1;
		else if(link->node->dirty && link->node->job->status != JOBSTATUS_DONE)
			return 0;
	}

	/* check if we are broken and propagate the result */
	if(broken)
	{
		job->status = JOBSTATUS_BROKEN;
		return 0;
	}

	/* if it doesn't have a tool, just mark it as done */
	if(!job->real)
	{
		job->status = JOBSTATUS_DONE;
		return 0;
	}
	
	/* check if constraints allows it */
	if(constraints_check(job))
		return 0;
	
	return 1;
}

/*
	searches the context for a job that we can do
*/
static struct JOB *find_job(struct CONTEXT *context)
{
	struct JOB *job;
	unsigned i;

	/* advance first_undone_job */
	while(context->first_undone_job < context->num_jobs &&
		context->joblist[context->first_undone_job]->status != JOBSTATUS_UNDONE)
	{
		context->first_undone_job++;
	}

	for(i = context->first_undone_job; i < context->num_jobs; i++)
	{
		job = context->joblist[i];
		if(check_job(context, job))
			return job;
	}

	return 0;
}

static void threads_run(void *u)
{
	struct THREADINFO *info = (struct THREADINFO *)u;
	struct CONTEXT *context = info->context;
	struct JOB *job;
	
	/* lock the dependency graph */
	criticalsection_enter();
	
	install_abort_signal();

	while(1)
	{
		if(session.abort)
			break;

		/* check if we are done */
		if(context->first_undone_job >= context->num_jobs)
			break;

		if(context->exit_on_error && context->errorcode)
			break;

		job = find_job(context);
		if(job)
		{
			run_job(context, job, info->id + 1);
		}
		else
		{
			/* if we didn't find a job todo, be a bit nice to the processor */
			/* TODO: we should wait for an event here */
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
	struct THREADINFO info[BAM_MAX_THREADS];
	void *threads[BAM_MAX_THREADS];
	int i;
	
	/* clamp number of threads */
	if(session.threads > BAM_MAX_THREADS)
	{
		printf("%s: reduced %d threads down to %d due to hard limit\n", session.name, session.threads, BAM_MAX_THREADS);
		printf("%s: change BAM_MAX_THREADS during compile to increase\n", session.name);
		session.threads = BAM_MAX_THREADS;
	}
	else if(session.threads < 1)
		session.threads = 1;
	
	for(i = 0; i < session.threads; i++)
	{
		info[i].context = context;
		info[i].id = i;
	}

	if(session.threads <= 1)
	{
		/* no threading, use this thread then */
		threads_run(&info[0]);
		if(session.report_bar)
			progressbar_clear();
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
	}
	return context->errorcode;
}

static int build_clean_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	
	/* no tool, no processing */
	if(node->job->real && node->timestamp)
	{
		if(remove(node->filename) == 0)
			printf("%s: removed '%s'\n", session.name, node->filename);
	}
	return 0;
}

int context_build_clean(struct CONTEXT *context)
{
	return node_walk(context->target, NODEWALK_BOTTOMUP|NODEWALK_FORCE|NODEWALK_QUICK, build_clean_callback, 0);
}

static int build_prepare_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct CONTEXT *context = (struct CONTEXT *)walkinfo->user;
	struct CACHENODE *cachenode;
	struct NODELINK *dep;
	struct NODELINK *parent;
	struct NODELINK *jobdep;
	struct NODEWALKPATH *path;

	time_t oldtimestamp = node->timestamp; /* to keep track of if this node changes */
	int olddirty = node->dirty;
	struct NODELINK *oldjobdep = node->job->firstjobdep;

	/* time sanity check */
	if(node->timestamp > context->buildtime)
		printf("%s: WARNING:'%s' comes from the future\n", session.name, node->filename);
	
	if(node->job->real)
	{
		/* dirty checking, check against cmdhash and global timestamp first */
		cachenode = cache_find_byhash(context->cache, node->hashid);
		if(cachenode)
		{
			node->job->cachehash = cachenode->cmdhash;
			if(node->job->cachehash != node->job->cmdhash)
				node->dirty = NODEDIRTY_CMDHASH;
		}
		else if(node->timestamp < context->globaltimestamp)
			node->dirty = NODEDIRTY_GLOBALSTAMP;
	}
	else if(node->timestamp_raw == 0)
	{
		printf("%s: error: '%s' does not exist and no way to generate it\n", session.name, node->filename);
		return 1;
	}
	
	/* check against all the dependencies */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(dep->node->job->real)
		{
			/* do circular action dependency checking */
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
		
			/* propagate job dependencies */
			node_job_add_dependency(node, dep->node);
		}
		else
		{
			/* propagate job dependencies */
			for(jobdep = dep->node->job->firstjobdep; jobdep; jobdep = jobdep->next)
				node_job_add_dependency(node, jobdep->node);
		}

		/* update dirty */		
		if(!node->dirty)
		{
			if(context->forced != 0)
				node->dirty = NODEDIRTY_FORCED;
			else if(dep->node->dirty)
				node->dirty = NODEDIRTY_DEPDIRTY;
			else if(node->timestamp < dep->node->timestamp)
			{
				if(node->job->real)
					node->dirty = NODEDIRTY_DEPNEWER;
				else /* no cmdline, just propagate the timestamp */
					node->timestamp = dep->node->timestamp;
			}
		}
	}

	/* mark as targeted */
	if(!walkinfo->revisiting)
		node->targeted = 1;
		
	/* invalidate the cache cmd hash if we are dirty because
		we could be dirty because if a dependency is missing */
	if(node->dirty && node->job->real)
		node->job->cachehash = 0;
	
	/* count commands */
	if(node->job->real && node->dirty && !node->job->counted && node->targeted)
	{
		node->job->counted = 1;

		/* add job to the list over jobs todo */
		context->joblist[context->num_jobs] = node->job;
		context->num_jobs++;
	}
	
	/* check if we should revisit it's parents to
		propagate the dirty state and timestamp.
		this can cause us to go outside the targeted
		nodes and into nodes that are not targeted. be aware */
	if(olddirty != node->dirty || oldtimestamp != node->timestamp || oldjobdep != node->job->firstjobdep)
	{
		for(parent = node->firstparent; parent; parent = parent->next)
			node_walk_revisit(walkinfo, parent->node);
	}

	return 0;
}

/* prepare does time sanity checking, dirty propagation,
	graph validation and job counting */
int context_build_prepare(struct CONTEXT *context)
{
	int error_code;

	/* create the job list */
	context->joblist = (struct JOB **)malloc(context->graph->num_jobs * sizeof(struct JOB *));

	/* revisit is used here to solve the problems
		where we have circular dependencies */
	error_code = node_walk(context->target,
		NODEWALK_BOTTOMUP|NODEWALK_FORCE|NODEWALK_REVISIT,
		build_prepare_callback, context);

	return error_code;
}

static int build_prioritize_callback(struct NODEWALK *walkinfo)
{
	struct JOB *job = walkinfo->node->job;
	struct NODELINK *link;

	job->priority++;
	
	/* propagate priority */
	for(link = job->firstjobdep; link; link = link->next)
		link->node->job->priority += job->priority;
	return 0;
}	

static int job_prio_compare(const void *a, const void *b)
{
	const struct JOB * const job_a = *(const struct JOB * const *)a;
	const struct JOB * const job_b = *(const struct JOB * const *)b;
	return job_b->priority - job_a->priority;
}

int context_build_prioritize(struct CONTEXT *context)
{
	int error_code;

	error_code = node_walk(context->target,
		NODEWALK_TOPDOWN|NODEWALK_FORCE|NODEWALK_QUICK|NODEWALK_JOBS,
		build_prioritize_callback, context);

	if(error_code)
		return error_code;

	/* sort the list */
	qsort(context->joblist, context->num_jobs, sizeof(struct JOB*), job_prio_compare);

	return error_code;
}
