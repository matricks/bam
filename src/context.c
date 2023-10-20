#include <stdio.h>
#include <stdlib.h> /* system() */
#include <string.h> /* strerror() */
#include <errno.h> /* errno */

#include "mem.h"
#include "context.h"
#include "path.h"
#include "node.h"
#include "cache.h"
#include "support.h"
#include "session.h"
#include "verify.h"

#ifndef BAM_MAX_THREADS
        #define BAM_MAX_THREADS 1024
#endif

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
		static char buf[128];
		int jobdigits = 0;
		int threaddigits = 0;
		int c;
		for(c = context->num_jobs; c; c /= 10)
			jobdigits++;

		for(c = session.threads; c; c /= 10)
			threaddigits++;
		
		if(session.report_color)
			sprintf(buf, "\033[01;32m[%%%dd/%%%dd] \033[01;36m[%%%dd]\033[00m %%s\n", jobdigits, jobdigits, threaddigits);
		else
			sprintf(buf, "[%%%dd/%%%dd] [%%%dd] %%s\n", jobdigits, jobdigits, threaddigits);
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

/*
	Makes sure that the job has updated the output timestamps correctly.
	If not, we touch the output ourself.
*/
static int verify_outputs(struct CONTEXT *context, struct JOB *job, time_t jobstarttime)
{
	struct NODELINK *link;
	time_t output_stamp;
	const char *reason = NULL;
	int errors = 0;

	/* make sure that the tool updated the output timestamps */
	for(link = job->firstoutput; link; link = link->next)
	{
		if(link->node->skipverifyoutput)
			continue;

		output_stamp = file_timestamp(link->node->filename);

		/* did the job update the timestamp correctly */
		reason = NULL;
		if(output_stamp == 0)
		{
			printf("%s: job '%s' did not produce expected output '%s'\n", session.name, job->label, link->node->filename);
			errors++;
		}
		else if(output_stamp == link->node->timestamp_raw)
			reason = "job did not update timestamp";
		else if(output_stamp < jobstarttime)
			reason = "timestamp was less then the job start timestamp";
		else if(output_stamp < link->node->timestamp)
			reason = "timestamp was less then the propagated timestamp";

		if(reason)
		{
			/* touch the file and get the new stamp */
			file_touch(link->node->filename);
			output_stamp = file_timestamp(link->node->filename);

			if(session.verbose)
			{
				printf("%s: output '%s' was touched. %s\n", session.name, link->node->filename, reason);
			}
		}

		/* set new timestamp */
		link->node->timestamp_raw = output_stamp;
	}

	return errors;
}

/*
	Checks so that a new file or updated file is actually a output or side effect of the job that ran
*/
static int verify_callback(const char *fullpath, hash_t hashid, time_t oldstamp, time_t newstamp, void *user) {
	struct JOB *job = user;

	if(oldstamp == 0 || oldstamp != newstamp)
	{
		struct NODELINK *link;
		struct STRINGLINK *slink;
		for(link = job->firstoutput; link; link = link->next)
			if(link->node->hashid == hashid)
				break;

		if(link == NULL)
		{
			for(slink = job->firstsideeffect; slink; slink = slink->next)
				if(strcmp(slink->str, fullpath) == 0)
					break;
		}

		if(link == NULL && slink == NULL)
		{
			if(oldstamp == 0)
				printf("%s: verification error: %s was created and not specified as an output\n", session.name, fullpath);
			else if(newstamp == 0)
				printf("%s: verification error: %s was deleted\n", session.name, fullpath);
			else
				printf("%s: verification error: %s was updated and not specified as an output\n", session.name, fullpath);
			return 1;
		}
	}

	return 0;
}

static int run_job(struct CONTEXT *context, struct JOB *job, int thread_id)
{
	struct NODELINK *link;
	int errorcode;
	time_t starttime;

	context->current_job_num++;

	/* mark the node as its in the working */
	job->status = JOBSTATUS_WORKING;

	/* print some nice information */
	runjob_print_report(context, job, thread_id);

	/* create output paths */
	if(runjob_create_outputpaths(job) != 0)
	{
		job->status = JOBSTATUS_BROKEN;
		return 1;
	}

	/* add constraints count */
	constraints_update(job, 1);
	
	event_begin(thread_id, "job", job->label);

	/* execute the command */
	criticalsection_leave();
	starttime = timestamp();
	errorcode = run_command(job->cmdline, job->filter);
	if(errorcode == 0)
	{
		/* make sure that the tool updated the timestamp and produced all outputs */
		errorcode = verify_outputs(context, job, starttime);
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

	/* run verify if requested */
	if(errorcode == 0 && context->verifystate != NULL)
	{
		event_begin(thread_id, "verify", job->label);
		errorcode = verify_update(context->verifystate, verify_callback, job);
		event_end(thread_id, "verify", NULL);
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
	if(!job->cmdline)
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
	int backofftime = 1;
	
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
			backofftime = 1;
			if(run_job(context, job, info->id + 1))
				context->errorcode = 1;
		}
		else
		{
			/* if we didn't find a job todo, be a bit nice to the processor */
			criticalsection_leave();
			/* TODO: we should wait for an event here */
			/* back off more and more up to 200ms */
			backofftime *= 2;
			if(backofftime > 200)
				backofftime = 200;
			threads_sleep(backofftime);
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

static int clean_file(const char *filename)
{
	if(remove(filename) == 0)
	{
		printf("%s: removed '%s'\n", session.name, filename);
		return 0;
	}
	else if(errno == ENOENT)
	{
		/* the error is "No such file or directory" which is fine */
		return 0;
	}
	else
	{
		printf("%s: error removing '%s': %s\n", session.name, filename, strerror(errno));
		return 1;
	}
}

static int build_clean_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct NODELINK *link;
	struct STRINGLINK *strlink;
	int result = 0;

	/* no tool, no processing */
	if(node->job->cmdline && !node->job->cleaned)
	{
		for(link = node->job->firstoutput; link; link = link->next)
			result |= clean_file(link->node->filename);

		for(strlink = node->job->firstclean; strlink; strlink = strlink->next)
			result |= clean_file(strlink->str);

		node->job->cleaned = 1;
	}

	return result;
}

int context_build_clean(struct CONTEXT *context)
{
	return node_walk(context->target, NODEWALK_BOTTOMUP|NODEWALK_FORCE|NODEWALK_QUICK|NODEWALK_NOABORT, build_clean_callback, 0);
}

static int build_prepare_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct CONTEXT *context = (struct CONTEXT *)walkinfo->user;
	struct CACHEINFO_OUTPUT *outputcacheinfo = NULL;
	struct NODELINK *dep;
	struct NODELINK *parent;
	struct NODELINK *jobdep;
	struct NODEWALKPATH *path;

	time_t oldtimestamp = node->timestamp; /* to keep track of if this node changes */
	int olddirty = node->dirty;
	struct NODELINK *oldjobdep = node->job->firstjobdep;

	/* 	time sanity check 
		files may have been written by the script, so compare to the global time after setup */
	if(node->timestamp > context->postsetuptime )
		printf("%s: warning:'%s' comes from the future\n", session.name, node->filename);
	
	if(node->job->cmdline)
	{
		/* dirty checking, check against cmdhash and global timestamp first */
		if(context->outputcache)
			outputcacheinfo = outputcache_find_byhash(context->outputcache, node->hashid);

		if(outputcacheinfo)
		{
			node->job->cachehash = outputcacheinfo->cmdhash;
			if(node->job->cachehash != node->job->cmdhash)
				node->dirty |= NODEDIRTY_CMDHASH;
		}
		else if(node->timestamp < context->globaltimestamp)
			node->dirty |= NODEDIRTY_GLOBALSTAMP;
	}
	else if(node->timestamp_raw == 0)
	{
		printf("%s: error: '%s' does not exist and no way to generate it\n", session.name, node->filename);
		return 1;
	}
	
	/* check against all the dependencies */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(dep->node->job->cmdline)
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
		if(context->forced != 0)
			node->dirty |= NODEDIRTY_FORCED;
		if(dep->node->dirty)
			node->dirty |= NODEDIRTY_DEPDIRTY;
		if(node->timestamp < dep->node->timestamp)
		{
			if(node->job->cmdline)
				node->dirty |= NODEDIRTY_DEPNEWER;
			else /* no cmdline, just propagate the timestamp */
				node->timestamp = dep->node->timestamp;
		}
	}

	/* mark as targeted */
	if(!walkinfo->revisiting)
		node->targeted = 1;
		
	if(node->dirty && node->job->cmdline)
	{
		/* invalidate the cache cmd hash if we are dirty because
			we could be dirty because if a dependency is missing */
		node->job->cachehash = 0;

		/* propagate dirty to our other outputs */
		for(dep = node->job->firstoutput; dep; dep = dep->next)
		{
			if(!dep->node->dirty)
			{
				dep->node->dirty |= node->dirty;
				for(parent = dep->node->firstparent; parent; parent = parent->next)
					node_walk_revisit(walkinfo, parent->node);				
			}
		}
	
		/* count commands */
		if(!node->job->counted && node->targeted)
		{
			node->job->counted = 1;

			/* add job to the list over jobs todo */
			context->joblist[context->num_jobs] = node->job;
			context->num_jobs++;
		}
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
	int64 prio_a = job_a->priority;
	int64 prio_b = job_b->priority;
	
	if(prio_b > prio_a)
		return 1;
	else if(prio_a > prio_b)
		return -1;
	else
		return 0;
}

static int build_prioritize_target_count_callback(struct NODEWALK *walkinfo)
{
	unsigned *counts = walkinfo->user;
	struct NODELINK *link;
	for(link = walkinfo->node->job->firstjobdep; link; link = link->next) {
		counts[link->node->id]++;
	}
	return 0;
}

static void prioritize_r(struct CONTEXT *context, struct NODE *node, unsigned *counts)
{
	struct JOB *job = node->job;
	struct NODELINK *link;

	/* propagate priority down the tree */
	job->priority++;
	for(link = job->firstjobdep; link; link = link->next)
	{
		link->node->job->priority += job->priority;

		/* remove a count and if the count is zero, all nodes above this one is done and
			we can prioritize this one now */
		counts[link->node->id]--;
		if(counts[link->node->id] == 0)
		{
			prioritize_r(context, link->node, counts);
		}
	}
}

int context_build_prioritize2(struct CONTEXT *context)
{
	unsigned *counts = malloc(context->graph->num_nodes * sizeof(unsigned));
	int error_code;
	memset(counts, 0, context->graph->num_nodes * sizeof(unsigned));

	/* store a count for every node for how many ways we will decend down onto it */
	error_code = node_walk(context->target,
		NODEWALK_TOPDOWN|NODEWALK_FORCE|NODEWALK_QUICK|NODEWALK_JOBS,
		build_prioritize_target_count_callback, counts);

	if(error_code)
	{
		free(counts);
		return error_code;
	}

	/* push the prio down the tree */
	prioritize_r(context, context->target, counts);

	/* sort the list */
	qsort(context->joblist, context->num_jobs, sizeof(struct JOB*), job_prio_compare);

	/* clean up */
	free(counts);

	return 0;
}

int context_build_prioritize1(struct CONTEXT *context)
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

extern int option_prio2;
int context_build_prioritize(struct CONTEXT *context)
{
	if(option_prio2)
		return context_build_prioritize2(context);
	return context_build_prioritize1(context);
}

void context_dump_joblist(struct CONTEXT *context)
{
	int i;
	struct JOB * job;
	struct NODELINK * link;

	printf("Priority Outputs\n");
	for(i = 0; i < context->num_jobs; i++)
	{
		job = context->joblist[i];
		link = job->firstoutput;
		printf("%16lld %s\n", job->priority, link->node->filename);
		link = link->next;
		for(; link; link = link->next)
			printf("         %s\n", link->node->filename);
	}
}
