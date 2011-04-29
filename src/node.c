#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lua.h>

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
	memcpy(str, src, len+1);
	return str;
}

/* */
struct GRAPH *node_graph_create(struct HEAP *heap)
{
	/* allocate graph structure */
	struct GRAPH *graph = (struct GRAPH*)mem_allocate(heap, sizeof(struct GRAPH));
	if(!graph)
		return (struct GRAPH *)0x0;
		
	memset(graph, 0, sizeof(struct GRAPH));

	/* init */
	graph->heap = heap;
	return graph; 
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
	job->real = 1;

	/*
	if(cmdline && !label)
	{
		printf("%s: error: adding job '%s' with command but no label\n", session.name, filename);
		return NODECREATE_INVALID_ARG;
	}
	else if(!cmdline && label)
	{
		printf("%s: error: adding job '%s' with label but no command\n", session.name, filename);
		return NODECREATE_INVALID_ARG;
	}*/

	/* set label and command */
	job->label = duplicate_string(graph, label, strlen(label));
	job->cmdline = duplicate_string(graph, cmdline, strlen(cmdline));
	job->cmdhash = string_hash(cmdline);
	job->cachehash = job->cmdhash;
	
	return job;
}

/* creates a node */
int node_create(struct NODE **nodeptr, struct GRAPH *graph, const char *filename, struct JOB *job)
{
	struct NODE *node;
	struct NODELINK *link;
	struct NODETREELINK *treelink;
	unsigned hashid = string_hash(filename);

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
		node->timestamp_raw = file_timestamp(filename);
		node->timestamp = node->timestamp_raw;
		
		if(node->timestamp_raw == 0)
			node->dirty = NODEDIRTY_MISSING;
		
		/* set filename */
		node->filename_len = strlen(filename)+1;
		node->filename = duplicate_string(graph, filename, node->filename_len);
		node->hashid = string_hash(filename);
		
		/* add to hashed tree */
		nodelinktree_insert(&graph->nodehash[node->hashid&0xffff], treelink, node);

		/* add to list */
		if(graph->last) graph->last->next = node;
		else graph->first = node;
		graph->last = node;		
	}

	/* set job */
	if(job)
	{
		if(node->job && node->job->real)
			return NODECREATE_EXISTS;
		
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
struct NODE *node_find_byhash(struct GRAPH *graph, unsigned int hashid)
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
		if(node_create(&node, graph, filename, NULL) == NODECREATE_OK)
			return node;
	}
	return node;
}

struct NODE *node_add_dependency_withnode(struct NODE *node, struct NODE *depnode)
{
	struct NODELINK *dep;
	struct NODELINK *parent;
	struct NODETREELINK *treelink;
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		if(node->job)
		{
			printf("error: node '%s' is depended on itself and is produced by a job\n", node->filename);
			return (struct NODE*)0x0;
		}
		
		return node;
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


struct NODE *node_job_add_dependency_withnode(struct NODE *node, struct NODE *depnode)
{
	struct NODELINK *dep;
	struct NODETREELINK *treelink;
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		if(node->job->real)
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


/* adds a dependency to a node */
struct NODE *node_add_dependency(struct NODE *node, const char *filename)
{
	struct NODE *depnode = node_get(node->graph, filename);
	if(!depnode)
		return NULL;
	return node_add_dependency_withnode(node, depnode);
}

static struct NODE *node_add_constraint(struct NODELINK **first, struct NODE *node, const char *filename)
{
	struct NODE *contraint = node_get(node->graph, filename);
	struct NODELINK *link = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	link->node = contraint;
	link->next = *first;
	*first = link;
	return contraint;
}

struct NODE *node_add_constraint_shared(struct NODE *node, const char *filename)
{
	return node_add_constraint(&node->constraint_shared, node, filename);
}

struct NODE *node_add_constraint_exclusive(struct NODE *node, const char *filename)
{
	return node_add_constraint(&node->constraint_exclusive, node, filename);
}

void node_cached(struct NODE *node)
{
	node->cached = 1;
}

void node_set_pseudo(struct NODE *node)
{
	node->timestamp = 1;
	node->timestamp_raw = 1;
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
		result = walk->callback(walk);
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
		if(result)
			break;
	}

	/* pop parent */
	walk->depth--;
	walk->parent = walk->parent->parent;
	
	/* unmark the node so we can walk this tree again if needed */
	if(!(flags&NODEWALK_QUICK))
		bitarray_clear(walk->mark, node->id);
	
	/* return if we have an error */
	if(result)
		return result;

	/* check if we need to rebuild this node */
	if(!(flags&NODEWALK_FORCE) && !node->dirty)
		return 0;
	
	/* build */
	if(flags&NODEWALK_BOTTOMUP)
	{
		walk->node = node;
		result = walk->callback(walk);
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
	
	/* do the walk of all active elements, if we don't have an error */
	if(flags&NODEWALK_REVISIT && !result)
	{
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

void node_debug_dump(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	struct NODELINK *link;

	for(;node;node = node->next)
	{
		printf("%s\n", node->filename);
		for(link = node->firstdep; link; link = link->next)
			printf("   DEPEND %s\n", link->node->filename);
	}
}

/* dumps all nodes to the stdout */
void node_debug_dump_detailed(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	struct NODELINK *link;
	const char *tool;
	
	for(;node;node = node->next)
	{
		static const char *dirtyflag[] = {"--", "MI", "CH", "DD", "DN", "GS"};
		tool = "***";
		if(node->job)
			tool = node->job->cmdline;
		printf("%08x %s   %s   %-15s\n", (unsigned)node->timestamp, dirtyflag[node->dirty], node->filename, tool);
		for(link = node->firstdep; link; link = link->next)
			printf("%08x %s      DEPEND %s\n", (unsigned)link->node->timestamp, dirtyflag[link->node->dirty], link->node->filename);
		for(link = node->firstparent; link; link = link->next)
			printf("%08x %s      PARENT %s\n", (unsigned)link->node->timestamp, dirtyflag[link->node->dirty], link->node->filename);
		for(link = node->constraint_shared; link; link = link->next)
			printf("%08x %s      SHARED %s\n", (unsigned)link->node->timestamp, dirtyflag[link->node->dirty], link->node->filename);
		for(link = node->constraint_exclusive; link; link = link->next)
			printf("%08x %s      EXCLUS %s\n", (unsigned)link->node->timestamp, dirtyflag[link->node->dirty], link->node->filename);
		for(link = node->job->firstjobdep; link; link = link->next)
			printf("%08x %s      JOBDEP %s\n", (unsigned)link->node->timestamp, dirtyflag[link->node->dirty], link->node->filename);
	}
}


static int node_debug_dump_dot_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct NODELINK *link;

	/* skip top node, always the build target */
	if(node == walkinfo->user)
		return 0;

	printf("node%d [label=\"%s\"];\n", node->id, node->filename);
	for(link = node->firstdep; link; link = link->next)
		printf("node%d -> node%d;\n", link->node->id, node->id);
	return 0;
}

/* dumps all nodes to the stdout */
void node_debug_dump_dot(struct GRAPH *graph, struct NODE *top)
{
	printf("digraph {\n");
	printf("graph [rankdir=\"LR\"];\n");
	printf("node [shape=box, height=0.25, color=gray, fontsize=8];\n");
	node_walk(top, NODEWALK_FORCE|NODEWALK_TOPDOWN|NODEWALK_QUICK, node_debug_dump_dot_callback, top);
	printf("}\n");
}

static int node_debug_dump_jobs_dot_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct NODELINK *link;

	/* skip top node, always the build target */
	if(node == walkinfo->user)
		return 0;

	printf("node%d [shape=box, label=\"%s\"];\n", node->id, node->filename);
	for(link = node->job->firstjobdep; link; link = link->next)
		printf("node%d -> node%d;\n", link->node->id, node->id);
	return 0;
}

void node_debug_dump_jobs_dot(struct GRAPH *graph, struct NODE *top)
{
	printf("digraph {\n");
	printf("graph [rankdir=\"LR\"];\n");
	printf("node [shape=box, height=0.25, color=gray, fontsize=8];\n");
	node_walk(top, NODEWALK_FORCE|NODEWALK_TOPDOWN|NODEWALK_JOBS|NODEWALK_QUICK, node_debug_dump_jobs_dot_callback, top);
	printf("}\n");
}

void node_debug_dump_jobs(struct GRAPH *graph)
{
	struct NODELINK *link;
	struct NODE *node = graph->first;
	static const char *dirtyflag[] = {"--", "MI", "CH", "DD", "DN", "GS"};
	printf("MI = Missing CH = Command hash dirty, DD = Dependency dirty\n");
	printf("DN = Dependency is newer, GS = Global stamp is newer\n");
	printf("Dirty Depth %-30s   Command\n", "Filename");
	for(;node;node = node->next)
	{
		if(node->job)
		{
			printf(" %s    %3d  %-30s   %s\n", dirtyflag[node->dirty], node->depth, node->filename, node->job->cmdline);
			
			for(link = node->job->firstjobdep; link; link = link->next)
				printf(" %s         + %-30s\n", dirtyflag[link->node->dirty], link->node->filename);
		}
	}
}
