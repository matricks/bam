#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "node.h"
#include "mem.h"
#include "support.h"
#include "path.h"
#include "context.h"
#include "session.h"

#include "nodelinktree.inl"


static char *duplicate_string(struct GRAPH *graph, const char *src, size_t len)
{
	char *str = (char *)mem_allocate(graph->heap, len+1);
	memcpy(str, src, len);
	str[len] = 0;
	return str;
}

/* */
struct GRAPH *node_graph_create(struct HEAP *heap)
{
	/* allocate graph structure */
	struct GRAPH *graph = (struct GRAPH*)mem_allocate(heap, sizeof(struct GRAPH));
	if(!graph)
		return (struct GRAPH *)0x0;

	/* init */
	graph->heap = heap;
	return graph; 
}

/*
	fetches the timestamp for the node and updates the dirty if the file is missing
*/
static void node_stat(struct NODE *node)
{
	node->timestamp_raw = file_timestamp(node->filename);
	node->timestamp = node->timestamp_raw;
	if(node->timestamp_raw == 0)
		node->dirty |= NODEDIRTY_MISSING;
}

static void stat_thread(void *user)
{
	struct GRAPH *graph = (struct GRAPH *)user;
	struct NODE *node = NULL;
	unsigned count = 0;
	unsigned loops = 0;

	while(1)
	{
		/* find the next stat node */
		struct NODE *next = NULL;
		if(node)
			next = node->nextstat;
		else
			next = graph->firststatnode;

		loops++;

		if(next)
		{
			count++;
			node = next;

			/* stat the node */
			node_stat(node);
			sync_barrier();
		}
		else
		{
			/* check if we should finish */
			if(node == graph->finalstatnode)
				break;

			/* be a little nice for now */
			threads_yield();
		}
	}
}

void node_graph_start_statthread(struct GRAPH *graph)
{
	graph->finalstatnode = (struct NODE *)0x1;
	sync_barrier();
	graph->statthread = threads_create(stat_thread, graph);
}

void node_graph_end_statthread(struct GRAPH *graph)
{
	graph->finalstatnode = graph->laststatnode;
	sync_barrier();
	threads_join(graph->statthread);
	graph->statthread = NULL;
}


struct JOB *node_job_create_null(struct GRAPH *graph)
{
	struct JOB *job = (struct JOB *)mem_allocate(graph->heap, sizeof(struct JOB));
	job->graph = graph;
	return job;
}


struct JOB *node_job_create(struct GRAPH *graph, const char *label, const char *cmdline)
{
	struct JOB *job = node_job_create_null(graph);
	graph->num_jobs++;

	/* set label and command */
	job->id = graph->num_jobs;
	job->label = duplicate_string(graph, label, strlen(label));
	job->cmdline = duplicate_string(graph, cmdline, strlen(cmdline));
	job->cmdhash = string_hash(cmdline);
	job->cachehash = job->cmdhash;

	/* add it to the list */
	job->next = graph->firstjob;
	graph->firstjob = job;
	
	return job;
}

/* creates a node */
int node_create(struct NODE **nodeptr, struct GRAPH *graph, const char *filename, struct JOB *job, time_t timestamp)
{
	struct NODE *node;
	struct NODELINK *link;
	struct NODETREELINK *treelink;
	hash_t hashid = string_hash(filename);

	/* check arguments */
	if(!path_isnice(filename))
	{
		printf("%s: error: adding non nice path '%s'. this causes problems with dependency lookups\n", session.name, filename);
		return NODECREATE_NOTNICE;
	}
	
	/* zero out the return pointer */
	*nodeptr = (struct NODE *)0x0;
		
	/* search for the node */
	treelink = nodelinktree_find_closest(graph->nodehash[hashid&0xffff], hashid);
	if(treelink && treelink->node->hashid == hashid)
	{
		/* we are allowed to create a new node from a node that doesn't
			have a job assigned to it*/
		/*if(link->node->cmdline || cmdline == NULL)
			return NODECREATE_EXISTS;*/
		node = treelink->node;
	}
	else
	{
		/* allocate and set pointers */
		node = (struct NODE *)mem_allocate(graph->heap, sizeof(struct NODE));
		
		node->graph = graph;
		node->id = graph->num_nodes++;
		
		/* set filename */
		node->filename_len = strlen(filename)+1;
		node->filename = duplicate_string(graph, filename, node->filename_len);
		node->hashid = hashid;
		
		/* add to hashed tree */
		nodelinktree_insert(&graph->nodehash[node->hashid&0xffff], treelink, node);

		/* add to list */
		if(graph->last) graph->last->next = node;
		else graph->first = node;
		graph->last = node;		

		/* fix timestamp */
		if(timestamp >= 0)
		{
			node->timestamp = timestamp;
			node->timestamp_raw = timestamp;
		}
		else
		{
			if(graph->statthread)
			{
				/* make sure that the node is written down before we queue it for a stat */
				sync_barrier();
				if(graph->laststatnode)
					graph->laststatnode->nextstat = node;
				else
					graph->firststatnode = node;
				graph->laststatnode = node;	
			}
			else
			{
				/* no stat-thread running, do it here and now */
				node_stat(node);
			}
		}
	}


	/* set job */
	if(job)
	{
		if(node->job && node->job->cmdline)
		{
			printf("%s: error: job '%s' already exists\n", session.name, filename);
			return NODECREATE_EXISTS;
		}

		/* TODO: we might have to transfer properties from the old job to the new? */
		node->job = job;

		/* link into output list */
		link = (struct NODELINK *)mem_allocate(graph->heap, sizeof(struct NODELINK));
		link->node = node;
		link->next = job->firstoutput;
		job->firstoutput = link;
	}
	else
		node->job = node_job_create_null(graph);

	/* return new node */
	*nodeptr = node;
	return NODECREATE_OK;
}

/* finds a node based apun the filename */
struct NODE *node_find_byhash(struct GRAPH *graph, hash_t hashid)
{
	struct NODETREELINK *link;
	link = nodelinktree_find_closest(graph->nodehash[hashid&0xffff], hashid);
	if(link && link->node->hashid == hashid)
		return link->node;
	return NULL;
}

struct NODE *node_find(struct GRAPH *graph, const char *filename)
{
	return node_find_byhash(graph, string_hash(filename));
}

/* this will return the existing node or create a new one */
struct NODE *node_get(struct GRAPH *graph, const char *filename) 
{
	struct NODE *node = node_find(graph, filename);
	
	if(!node)
	{
		if(node_create(&node, graph, filename, NULL, TIMESTAMP_NONE) == NODECREATE_OK)
			return node;
	}
	return node;
}

struct NODE *node_add_dependency (struct NODE *node, struct NODE *depnode)
{
	struct NODELINK *dep;
	struct NODELINK *parent;
	struct NODETREELINK *treelink;
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		if(node->job->cmdline)
		{
			printf("error: node '%s' is depended on itself and is produced by a job\n", node->filename);
			return (struct NODE*)0x0;
		}
		
		return node;
	}

	/* */
	if(node->job == depnode->job && node->job->cmdline)
	{
		printf("error: node '%s' is depended on '%s' and they are produced by the same job\n", node->filename, depnode->filename);
		return (struct NODE*)0x0;
	}
	
	/* check if we are already dependent on this node */
	treelink = nodelinktree_find_closest(node->deproot, depnode->hashid);
	if(treelink != NULL && treelink->node->hashid == depnode->hashid)
		return depnode;

	/* create and add dependency link */
	dep = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	dep->node = depnode;
	dep->next = node->firstdep;
	node->firstdep = dep;
	
	nodelinktree_insert(&node->deproot, treelink, depnode);
	
	/* create and add parent link */
	parent = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	parent->node = node;
	parent->next = depnode->firstparent;
	depnode->firstparent = parent;
	
	/* increase dep counter */
	node->graph->num_deps++;
		
	/* return the dependency */
	return depnode;
}


struct NODE *node_job_add_dependency (struct NODE *node, struct NODE *depnode)
{
	struct NODELINK *dep;
	struct NODETREELINK *treelink;
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		if(node->job->cmdline)
		{
			printf("error: node '%s' is depended on itself and is produced by a job\n", node->filename);
			return (struct NODE*)0x0;
		}
		
		return node;
	}

	/* check if we are already dependent on this node */
	treelink = nodelinktree_find_closest(node->job->jobdeproot, depnode->hashid);
	if(treelink != NULL && treelink->node->hashid == depnode->hashid)
		return depnode;
	
	/* create and add job dependency link */
	dep = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	dep->node = depnode;
	dep->next = node->job->firstjobdep;
	node->job->firstjobdep = dep;
	
	nodelinktree_insert(&node->job->jobdeproot, treelink, depnode);	
	
	return depnode;
}

int node_add_clean(struct NODE *node, const char * filename)
{
	struct STRINGLINK * link;
	if(!node->job->cmdline)
		return -1;

	/* create and add clean link */
	link = (struct STRINGLINK *)mem_allocate(node->graph->heap, sizeof(struct STRINGLINK));
	link->str = duplicate_string(node->graph, filename, strlen(filename));
	link->next = node->job->firstclean;
	node->job->firstclean = link;
	return 0;
}

int node_add_sideeffect(struct NODE *node, const char * filename)
{
	struct STRINGLINK * link;
	if(!node->job->cmdline)
		return -1;

	/* create and add sideffect link */
	link = (struct STRINGLINK *)mem_allocate(node->graph->heap, sizeof(struct STRINGLINK));
	link->str = duplicate_string(node->graph, filename, strlen(filename));
	link->next = node->job->firstsideeffect;
	node->job->firstsideeffect = link;
	return 0;
}

/* adds a dependency to a node */
static struct NODE *node_add_constraint (struct NODELINK **first, struct NODE *node, struct NODE *contraint)
{
	struct NODELINK *link = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	link->node = contraint;
	link->next = *first;
	*first = link;
	return contraint;
}

struct NODE *node_add_constraint_shared (struct NODE *node, struct NODE *contraint)
{
	return node_add_constraint (&node->constraint_shared, node, contraint);
}

struct NODE *node_add_constraint_exclusive (struct NODE *node, struct NODE *contraint)
{
	return node_add_constraint (&node->constraint_exclusive, node, contraint);
}

void node_cached(struct NODE *node)
{
	node->cached = 1;
}

/* functions to handle with bit array access */
static unsigned char *bitarray_allocate(int size)
{ return (unsigned char *)malloc((size+7)/8); }

static void bitarray_zeroall(unsigned char *a, int size)
{ memset(a, 0, (size+7)/8); }

static void bitarray_free(unsigned char *a)
{ free(a); }

static int bitarray_value(unsigned char *a, int id)
{ return a[id>>3]&(1<<(id&0x7)); }

static void bitarray_set(unsigned char *a, int id)
{ a[id>>3] |= (1<<(id&0x7)); }

static void bitarray_clear(unsigned char *a, int id)
{ a[id>>3] &= ~(1<<(id&0x7)); }

/* ************* */
static int node_walk_r(
	struct NODEWALK *walk,
	struct NODE *node)
{
	/* we should detect changes here before we run */
	struct NODELINK *dep;
	struct NODEWALKPATH path;
	int result = 0;
	int flags = walk->flags;
	
	/* check and set mark */
	if(bitarray_value(walk->mark, node->id))
		return 0; 
	bitarray_set(walk->mark, node->id);
	
	if(flags&NODEWALK_UNDONE)
	{
		if(node->job->status != JOBSTATUS_UNDONE)
			return 0;
	}

	if(flags&NODEWALK_TOPDOWN)
	{
		walk->node = node;
		result |= walk->callback(walk);
	}

	/* push parent */
	path.node = node;
	path.parent = walk->parent;
	walk->parent = &path;
	walk->depth++;
	
	/* build all dependencies */
	dep = node->firstdep;
	if(flags&NODEWALK_JOBS)
		dep = node->job->firstjobdep;
	for(; dep; dep = dep->next)
	{
		result = node_walk_r(walk, dep->node);
		if(!(flags&NODEWALK_NOABORT) && result)
			break;
	}

	/* pop parent */
	walk->depth--;
	walk->parent = walk->parent->parent;
	
	/* unmark the node so we can walk this tree again if needed */
	if(!(flags&NODEWALK_QUICK))
		bitarray_clear(walk->mark, node->id);
	
	/* return if we have an error */
	if(!(flags&NODEWALK_NOABORT) && result)
		return result;

	/* check if we need to rebuild this node */
	if(!(flags&NODEWALK_FORCE) && !node->dirty)
		return 0;
	
	/* build */
	if(flags&NODEWALK_BOTTOMUP)
	{
		walk->node = node;
		result |= walk->callback(walk);
	}
	
	return result;
}

/* walks through all the active nodes that needs a recheck */
static int node_walk_do_revisits(struct NODEWALK *walk)
{
	int result;
	struct NODE *node;
	
	/* no parent or depth info is available */
	walk->parent = NULL;
	walk->depth = 0;
	walk->revisiting = 1;

	while(walk->firstrevisit)
	{
		/* pop from the list */
		node = walk->firstrevisit->node;
		walk->firstrevisit->node = NULL;
		walk->firstrevisit = walk->firstrevisit->next;
		
		/* issue the call */
		walk->node = node;
		result = walk->callback(walk);
		if(result)
			return result;
	}
	
	/* return success */
	return 0;
}

int node_walk(
	struct NODE *node,
	int flags,
	int (*callback)(struct NODEWALK*),
	void *u)
{
	struct NODEWALK walk;
	int result;
	
	/* set walk parameters */
	walk.depth = 0;
	walk.flags = flags;
	walk.callback = callback;
	walk.user = u;
	walk.parent = 0;
	walk.revisiting = 0;
	walk.firstrevisit = NULL;
	walk.revisits = NULL;

	/* allocate and clear mark and sweep array */
	walk.mark = bitarray_allocate(node->graph->num_nodes);
	bitarray_zeroall(walk.mark, node->graph->num_nodes);
	
	/* allocate memory for activation */
	if(flags&NODEWALK_REVISIT)
	{
		walk.revisits = malloc(sizeof(struct NODEWALKREVISIT)*node->graph->num_nodes);
		memset(walk.revisits, 0, sizeof(struct NODEWALKREVISIT)*node->graph->num_nodes);
	}

	/* do the walk */
	result = node_walk_r(&walk, node);
	
	/* do the walk of all active elements, if we don't have an error and free the memory */
	if(flags&NODEWALK_REVISIT)
	{
		if(!result)
			node_walk_do_revisits(&walk);
		free(walk.revisits);
	}

	/* free the array and return */
	bitarray_free(walk.mark);
	return result;
}

void node_walk_revisit(struct NODEWALK *walk, struct NODE *node)
{
	struct NODEWALKREVISIT *revisit = &walk->revisits[node->id];
	
	/* check if node already marked for revisit */
	if(revisit->node)
		return;
	
	/* no need to revisit the node if there is a visit to be done for it */
	/* TODO: the necessarily of this check is unknown. should check some larger builds to see
			if it helps any substantial amount. */
	if(!walk->revisiting && !bitarray_value(walk->mark, node->id))
		return;
	
	/* insert the node to the nodes to revisit */
	revisit->node = node;
	revisit->next = walk->firstrevisit;
	walk->firstrevisit = revisit;
}

static const char *dirtyflags_str(unsigned dirty)
{
	static char field[NODEDIRTY_NUMFLAGS+1];
	/* start with all flags set */
	unsigned i = 1;
	field[0] = 'm';
	field[1] = 'c';
	field[2] = 'd';
	field[3] = 'n';
	field[4] = 'g';
	field[5] = 'f';
	field[6] = 0;

	/* clear out flags */
	for(i = 0; i < NODEDIRTY_NUMFLAGS; i++)
		if((dirty&(1<<i)) == 0)
			field[i] = '-';
	return field;
}

static const char *dirtyflags_str_empty = "      ";
static const char *dirtyflags_help =
	"Dirty explanation:\n"
	"m = missing file, c = command line changed, d = a dependecy is dirty\n"
	"n = a dependecy is newer, g = global timestamp is newer, f = forced\n";

static const char *decorate_link(unsigned id, const char *name, const char *linktype, int html) {
	static char buffer[4*1024];
	if(!html)
		return name;
	snprintf(buffer, sizeof(buffer), "<a href=\"#%s%u\">%s</a>", linktype, id, name);
	return buffer;
}

static const char *decorate_header(unsigned id, const char *name, const char *linktype, int html) {
	static char buffer[4*1024];
	if(!html)
		return name;
	snprintf(buffer, sizeof(buffer), "<a id=\"%s%u\" href=\"#%s%u\">%s</a>", linktype, id, linktype, id, name);
	return buffer;
}

/* dumps all nodes to the stdout */
void node_debug_dump(struct GRAPH *graph, int html)
{
	struct JOB *job = graph->firstjob;
	struct NODE *node = graph->first;
	struct NODELINK *link;
	struct STRINGLINK *strlink;

	if(html)
	{
		printf("<html><body><pre>\n");
		printf("%s", dirtyflags_help);
	}


	printf("Dirty Prio %-30s Command\n", "Label");
	for(;job;job = job->next)
	{
		printf("%8s %s JOB %-30s %s\n", "", dirtyflags_str_empty, decorate_header(job->id, job->label, "j", html), job->cmdline);
		
		for(link = job->firstoutput; link; link = link->next)
			printf("%08x %s    OUTPUT %-30s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));

		for(link = job->firstjobdep; link; link = link->next)
			printf("%8s %s    JOBDEP %-30s\n", "", dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));

		for(strlink = job->firstclean; strlink; strlink = strlink->next)
			printf("%8s %s    CLEAN  %-30s\n", "", dirtyflags_str_empty, strlink->str);

		for(strlink = job->firstsideeffect; strlink; strlink = strlink->next)
			printf("%8s %s    SIDE  %-30s\n", "", dirtyflags_str_empty, strlink->str);
		printf("\n");
	}


	for(;node;node = node->next)
	{
		printf("%08x %s NODE %s\n", (unsigned)node->timestamp, dirtyflags_str(node->dirty), decorate_header(node->id, node->filename, "n", html));
		if(node->job->cmdline)
			printf( "%8s %s    JOB    %s\n", "", dirtyflags_str_empty, decorate_link(node->job->id, node->job->label, "j", html));
		for(link = node->firstdep; link; link = link->next)
			printf("%08x %s    DEPEND %s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));
		for(link = node->firstparent; link; link = link->next)
			printf("%08x %s    PARENT %s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));
		for(link = node->constraint_shared; link; link = link->next)
			printf("%08x %s    SHARED %s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));
		for(link = node->constraint_exclusive; link; link = link->next)
			printf("%08x %s    EXCLUS %s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));
		/*
		for(link = node->job->firstjobdep; link; link = link->next)
			printf("%08x %s       JOBDEP %s\n", (unsigned)link->node->timestamp, dirtyflags_str(link->node->dirty), decorate_link(link->node->id, link->node->filename, "n", html));
		for(slink = node->job->firstclean; slink; slink = slink->next)
			printf("%8s %s       CLEAN  %s\n", "", dirtyflags_str_empty, slink->str);
		for(slink = node->job->firstsideeffect; slink; slink = slink->next)
			printf("%8s %s       SIDE   %s\n", "", dirtyflags_str_empty, slink->str);
			*/
		printf("\n");
	}

	if(html)
		printf("</pre></body></html>\n");
	else
		printf("%s", dirtyflags_help);

}

void node_debug_dump_jobs(struct GRAPH *graph)
{
	struct NODELINK *link;
	struct STRINGLINK *strlink;
	struct JOB *job = graph->firstjob;

	printf("Dirty Prio %-30s Command\n", "Label");
	for(;job;job = job->next)
	{
		printf(" %s   %4d %-30s %s\n", dirtyflags_str_empty, job->priority, job->label, job->cmdline);
		
		for(link = job->firstoutput; link; link = link->next)
			printf(" %s          OUTPUT %-30s\n", dirtyflags_str(link->node->dirty), link->node->filename);

		for(link = job->firstjobdep; link; link = link->next)
			printf(" %s          DEPEND %-30s\n", dirtyflags_str(link->node->dirty), link->node->filename);

		for(strlink = job->firstclean; strlink; strlink = strlink->next)
			printf(" %s          CLEAN  %-30s\n", dirtyflags_str_empty, strlink->str);

		for(strlink = job->firstsideeffect; strlink; strlink = strlink->next)
			printf(" %s          SIDE  %-30s\n", dirtyflags_str_empty, strlink->str);
	}

	printf("%s", dirtyflags_help);
}
