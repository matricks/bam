#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lua.h>

#include "node.h"
#include "mem.h"
#include "support.h"
#include "path.h"

#include "tree.h"

#ifdef USE_NODE_RB
	RB_HEAD(NODERB, NODE);

	static int node_cmp(struct NODE *a, struct NODE *b)
	{
		return a->hashid - b->hashid;
	}

	RB_GENERATE_INTERNAL(NODERB, NODE, rbentry, node_cmp, static)

	void FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
	{
		(void)NODERB_RB_REMOVE;
		(void)NODERB_RB_NFIND;
		(void)NODERB_RB_NEXT;
		(void)NODERB_RB_PREV;
		(void)NODERB_RB_MINMAX;
	}
#endif

/**/
struct GRAPH
{
#ifdef USE_NODE_RB
	struct NODERB nodehash[0x10000];
#else
	struct NODE *nodehash[0x10000];
#endif
	struct NODE *first;
	struct HEAP *heap;
	int num_nodes;
};

/* */
static unsigned int string_hash(const char *str)
{
	unsigned int h = 0;
	for (; *str; str++)
		h = 31*h + *str;
	return h;
}



/**/
struct GRAPH *node_create_graph(struct HEAP *heap)
{
	/* allocate graph structure */
	struct GRAPH *graph = (struct GRAPH*)mem_allocate(heap, sizeof(struct GRAPH));
	if(!graph)
		return (struct GRAPH *)0x0;

	/* init */
	graph->heap = heap;
	graph->num_nodes = 0;
	graph->first = (struct NODE*)0x0;
	memset(graph->nodehash, 0, sizeof(struct NODE*)*0x10000);
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
	node->id = graph->num_nodes++;
	node->timestamp = file_timestamp(filename);
	node->firstdep = (struct DEPENDENCY*)0x0;
	node->firstscanner = (struct SCANNER*)0x0;
	
	/* set filename */
	sn = strlen(filename)+1;
	node->filename = (char *)mem_allocate(graph->heap, sn);
	memcpy(node->filename, filename, sn);
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
	}
		
	/* add to hashed tree */
#ifdef USE_NODE_RB
	RB_INSERT(NODERB, &graph->nodehash[node->hashid&0xffff], node);
#else
	node->hashnext = graph->nodehash[node->hashid&0xffff];
	graph->nodehash[node->hashid&0xffff] = node;
#endif

	/* add to list */
	node->next = graph->first;
	graph->first = node;
	
	/* zero out flags */
	node->dirty = 0;
	node->depchecked = 0;
	node->cached = 0;
	node->parenthastool = 0;
	node->counted = 0;
	node->workstatus = NODESTATUS_UNDONE;
	
	/* return new node */
	*nodeptr = node;
	return NODECREATE_OK;
}

/* finds a node based apun the filename */
struct NODE *node_find(struct GRAPH *graph, const char *filename)
{
	unsigned int hashid = string_hash(filename);
#ifdef USE_NODE_RB
	struct NODE tempnode;
	tempnode.hashid = hashid;
	return RB_FIND(NODERB, &graph->nodehash[hashid&0xffff], &tempnode);
#else	
	struct NODE *node = graph->nodehash[hashid&0xffff];
	for(;node;node = node->hashnext)
	{
		if(node->hashid == hashid)
		{
			if(strcmp(node->filename, filename) == 0)
				return node;
		}
	}
	return (struct NODE *)0x0;
#endif
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

/* adds a dependency to a node */
struct NODE *node_add_dependency(struct NODE *node, const char *filename)
{
	struct NODE *depnode;
	struct DEPENDENCY *dep;
	
	/* get node (can't fail) */
	depnode = node_get(node->graph, filename);
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		printf("error: this file depends on it self\n  %s\n", node->filename);
		return (struct NODE*)0x0;
	}
	
	/* create dependency */
	dep = (struct DEPENDENCY *)mem_allocate(node->graph->heap, sizeof(struct DEPENDENCY));
	dep->node = depnode;
	
	/* add the dependency to the node */
	dep->next = node->firstdep;
	node->firstdep = dep;
	
	/* set parenttooldep */
	if(node->cmdline)
		depnode->parenthastool = 1;

	/* return the dependency */
	return depnode;
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
	struct DEPENDENCY *dep;
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

	/* allocate and clear mark and sweep array */
	walk.mark = bitarray_allocate(node->graph->num_nodes);
	bitarray_zeroall(walk.mark, node->graph->num_nodes);

	/* do the walk */
	result = node_walk_r(&walk, node);

	/* free the array and return */
	bitarray_free(walk.mark);
	return result;
}

/* dumps all nodes to the stdout */
void node_debug_dump(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	struct DEPENDENCY *dep;
	const char *tool;
	/*const char *workstatus = "UWD";*/
	for(;node;node = node->next)
	{
		static const char d[] = " D";
		tool = "***";
		if(node->cmdline)
			tool = node->cmdline;
		printf("%08x %c   %s   %-15s\n", (unsigned)node->timestamp, d[node->dirty], node->filename, tool);
		for(dep = node->firstdep; dep; dep = dep->next)
			printf("%08x %c      %s\n", (unsigned)dep->node->timestamp, d[dep->node->dirty], dep->node->filename);
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

/* dumps all nodes to the stdout in dot format (graphviz) */
static int node_debug_dump_dot_r(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct DEPENDENCY *dep;

	if(!node->parenthastool && node != walkinfo->user)
		return 0;
	
	if(node->cmdline)
	{
		for(dep = node->firstdep; dep; dep = dep->next)
			printf("\tn%d -> n%d\n", dep->node->id, node->id);
	}
		
	return 0;
}

void node_debug_dump_dot(struct GRAPH *graph, struct NODE *root_node)
{
	printf("digraph G {\n");
	printf("\tnodesep=.05;");
	printf("\tconcentrate=true;\n");
	printf("\tnode[fontsize=8];\n");

	{
		struct PATH
		{
			struct PATH *next;
			char name[1];
		};

		struct NODE *node;
		struct PATH *first = 0;
		struct PATH *cur;
		char directory[512];
		int found;

		for(node = graph->first; node; node = node->next)
		{
			if(!node->parenthastool && node != root_node)
				continue;
			
			path_directory(node->filename, directory, sizeof(directory));

			found = 0;
			for(cur = first; cur; cur = cur->next)
				if(strcmp(cur->name, directory) == 0)
				{
					found = 1;
					break;
				}

			if(!found)
			{
				cur = (struct PATH*)malloc(sizeof(struct PATH) + strlen(directory));
				cur->next = first;
				strcpy(cur->name, directory);
				first = cur;
			}
		}

		for(cur = first; cur; cur = cur->next)
		{
			printf("subgraph \"cluster_%s\" {\n", cur->name);
			for(node = graph->first; node; node = node->next)
			{
				if(!node->parenthastool && node != root_node)
					continue;
					
				path_directory(node->filename, directory, sizeof(directory));
				if(strcmp(directory, cur->name) == 0)
					printf("\tn%d [label=\"%s\\n%s\"];\n", node->id, path_filename(node->filename), node->cmdline);
			}
			
			printf("}\n");
			
		}
	}

	node_walk(root_node, NODEWALK_FORCE|NODEWALK_BOTTOMUP|NODEWALK_QUICK, node_debug_dump_dot_r, root_node);
	printf("}\n");	
}
