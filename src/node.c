#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lua.h>

#include "node.h"
#include "mem.h"
#include "support.h"
#include "path.h"
#include "context.h"

static int node_cmp(struct NODE *a, struct NODE *b)
{
	if(a->hashid > b->hashid) return 1;
	if(a->hashid < b->hashid) return -1;
	return 0;
}

RB_GENERATE_INTERNAL(NODERB, NODE, rbentry, node_cmp, static)

void NODE_FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
{
	(void)NODERB_RB_REMOVE; (void)NODERB_RB_NFIND; (void)NODERB_RB_MINMAX;
	(void)NODERB_RB_PREV; (void)NODERB_RB_NEXT;
}


/* */
static unsigned int string_hash(const char *str)
{
	unsigned int h = 0;
	for (; *str; str++)
		h = 31*h + *str;
	return h;
}

/* */
struct GRAPH *node_create_graph(struct HEAP *heap)
{
	/* allocate graph structure */
	struct GRAPH *graph = (struct GRAPH*)mem_allocate(heap, sizeof(struct GRAPH));
	if(!graph)
		return (struct GRAPH *)0x0;

	/* init */
	memset(graph, 0, sizeof(struct GRAPH));
	graph->heap = heap;
	return graph; 
}

struct HEAP *node_graph_heap(struct GRAPH *graph)
{
	return graph->heap;
}

/* creates a node */
int node_create(struct NODE **nodeptr, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline)
{
	struct NODE *node;
	int sn;

	if(!path_isnice(filename))
		printf("WARNING: adding non nice path %s\n", filename);
	
	/* zero out the return pointer */
	*nodeptr = (struct NODE *)0x0;
	
	/* */
	if(!path_isnice(filename))
		return NODECREATE_NOTNICE;
		
	/* */
	if(node_find(graph, filename))
		return NODECREATE_EXISTS;
	
	/* allocate and set pointers */
	node = (struct NODE *)mem_allocate(graph->heap, sizeof(struct NODE));
	node->graph = graph;
	node->depth = 0;
	node->id = graph->num_nodes++;
	node->timestamp = file_timestamp(filename);
	node->firstdep = (struct NODELINK*)0x0;
	node->firstparent = (struct NODELINK*)0x0;
	
	/* set filename */
	node->filename_len = strlen(filename)+1;
	node->filename = (char *)mem_allocate(graph->heap, node->filename_len);
	memcpy(node->filename, filename, node->filename_len);
	node->hashid = string_hash(filename);

	/* set label line */
	node->label = 0;
	if(label && label[0])
	{
		sn = strlen(label)+1;
		node->label = (char *)mem_allocate(graph->heap, sn);
		memcpy(node->label, label, sn);
	}

	/* set cmdline line */
	node->cmdline = 0;
	if(cmdline && cmdline[0])
	{
		sn = strlen(cmdline)+1;
		node->cmdline = (char *)mem_allocate(graph->heap, sn);
		memcpy(node->cmdline, cmdline, sn);
		node->cmdhash = string_hash(cmdline);
	}
		
	/* add to hashed tree */
	RB_INSERT(NODERB, &graph->nodehash[node->hashid&0xffff], node);

	/* add to list */
	if(graph->last) graph->last->next = node;
	else graph->first = node;
	node->next = 0;
	graph->last = node;
	
	/* zero out flags */
	node->dirty = 0;
	node->depchecked = 0;
	node->cached = 0;
	node->parenthastool = 0;
	node->counted = 0;
	node->isdependedon = 0;
	node->workstatus = NODESTATUS_UNDONE;
	
	/* return new node */
	*nodeptr = node;
	return NODECREATE_OK;
}

/* finds a node based apun the filename */
struct NODE *node_find(struct GRAPH *graph, const char *filename)
{
	unsigned int hashid = string_hash(filename);
	struct NODE tempnode;
	tempnode.hashid = hashid;
	return RB_FIND(NODERB, &graph->nodehash[hashid&0xffff], &tempnode);
}

/* this will return the existing node or create a new one */
struct NODE *node_get(struct GRAPH *graph, const char *filename) 
{
	struct NODE *node = node_find(graph, filename);
	if(!node)
	{
		if(node_create(&node, graph, filename, 0, 0) == NODECREATE_OK)
			return node;
	}
	return node;
}

struct NODE *node_add_dependency_withnode(struct NODE *node, struct NODE *depnode)
{
	struct NODELINK *dep;
	struct NODELINK *parent;
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		printf("error: this file depends on it self\n  %s\n", node->filename);
		return (struct NODE*)0x0;
	}
	
	/* check if we are already dependent on this node */
	for(dep = node->firstdep; dep; dep = dep->next)
		if(dep->node->hashid == depnode->hashid)
			return depnode;
	
	/* create and add dependency link */
	dep = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	dep->node = depnode;
	dep->next = node->firstdep;
	node->firstdep = dep;
	
	/* create and add parent link */
	parent = (struct NODELINK *)mem_allocate(node->graph->heap, sizeof(struct NODELINK));
	parent->node = node;
	parent->next = depnode->firstparent;
	depnode->firstparent = parent;
	
	/* set depnode flags */
	if(node->cmdline)
		depnode->parenthastool = 1;
	depnode->isdependedon = 1;
	
	/* increase dep counter */
	node->graph->num_deps++;
		
	/* return the dependency */
	return depnode;
}

/* adds a dependency to a node */
struct NODE *node_add_dependency(struct NODE *node, const char *filename)
{
	return node_add_dependency_withnode(node, node_get(node->graph, filename));
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
	int needrebuild = 0;
	
	/* check and set mark */
	if(bitarray_value(walk->mark, node->id))
		return 0; 
	bitarray_set(walk->mark, node->id);
	
	if((walk->flags)&NODEWALK_UNDONE)
	{
		if(node->workstatus != NODESTATUS_UNDONE)
			return 0;
	}

	if((walk->flags)&NODEWALK_TOPDOWN)
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
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		result = node_walk_r(walk, dep->node);
		if(node->timestamp < dep->node->timestamp)
			needrebuild = 1;
		if(result)
			break;
	}

	/* pop parent */
	walk->depth--;
	walk->parent = walk->parent->parent;
	
	/* unmark the node so we can walk this tree again if needed */
	if(!(walk->flags&NODEWALK_QUICK))
		bitarray_clear(walk->mark, node->id);
	
	/* return if we have an error */
	if(result)
		return result;

	/* check if we need to rebuild this node */
	if(!((walk->flags)&NODEWALK_FORCE) && !node->dirty)
		return 0;
	
	/* build */
	if((walk->flags)&NODEWALK_BOTTOMUP)
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

/* dumps all nodes to the stdout */
void node_debug_dump(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	struct NODELINK *link;
	const char *tool;
	
	for(;node;node = node->next)
	{
		static const char d[] = " D";
		tool = "***";
		if(node->cmdline)
			tool = node->cmdline;
		printf("%08x %c   %s   %-15s\n", (unsigned)node->timestamp, d[node->dirty], node->filename, tool);
		for(link = node->firstdep; link; link = link->next)
			printf("%08x %c      D %s\n", (unsigned)link->node->timestamp, d[link->node->dirty], link->node->filename);
		for(link = node->firstparent; link; link = link->next)
			printf("%08x %c      P %s\n", (unsigned)link->node->timestamp, d[link->node->dirty], link->node->filename);
	}
}

void node_debug_dump_jobs(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	static const char *dirtyflag[] = {"--", "MI", "CH", "DD", "DN", "GS"};
	printf("MI = Missing CH = Command hash dirty, DD = Dependency dirty\n");
	printf("DN = Dependency is newer, GS = Global stamp is newer\n");
	printf("Dirty Depth %-30s   Command\n", "Filename");
	for(;node;node = node->next)
	{
		if(node->cmdline)
			printf(" %s    %3d  %-30s   %s\n", dirtyflag[node->dirty], node->depth, node->filename, node->cmdline);
	}
}


static int node_debug_dump_tree_r(struct NODEWALK *walkinfo)
{
	/*const char *workstatus = "UWD";*/
	unsigned i;
	if(walkinfo->node->workstatus != NODESTATUS_DONE)
	{
		static const char d[] = " D";
		printf("%3d %08x %c ", walkinfo->depth, (unsigned)walkinfo->node->timestamp, d[walkinfo->node->dirty]);
		for(i = 0; i < walkinfo->depth; i++)
			printf("  ");
		printf("%s   %s\n", walkinfo->node->filename, walkinfo->node->cmdline);
	}
	return 0;
}

void node_debug_dump_tree(struct NODE *root)
{
	node_walk(root, NODEWALK_FORCE|NODEWALK_TOPDOWN, node_debug_dump_tree_r, 0);
}
