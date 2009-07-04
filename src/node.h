#ifndef FILE_NODE_H
#define FILE_NODE_H


/***** dependency graph *******/

#include <time.h>
#include "tree.h"

/**/
struct DEPENDENCY
{
	struct NODE *node;
	struct DEPENDENCY *next;
};

struct SCANNER
{
	struct SCANNER *next;
	int (*scannerfunc)(struct NODE *, struct SCANNER *info);
};


/*
	64 byte on a 32 machine.
	this should be able to fit inside a cache line
	or perhaps two in one cache line
	TODO: these should be allocated cache aligned
*/
struct NODE
{
	/* *** */
	struct GRAPH *graph; /* graph that the node belongs to */
	struct NODE *next; /* next node in the graph */
	struct DEPENDENCY *firstdep; /* list of dependencies */
	
	char *filename; /* this contains the filename with the FULLPATH */
	char *cmdline; /* command line that should be executed to build this node */
	char *label; /* what to print when we build this node */
	
	RB_ENTRY(NODE) rbentry; /* RB-tree entry sorted by hashid */

	/* filename and the tool to build the resource */
	/* unsigned filename_len; */ /* including zero term */
	unsigned cmdhash; /* hash of the command line for detecting changes */
	 
	unsigned hashid; /* hash of the filename/nodename */
	
	time_t timestamp; /* timestamp of the node, 0 == does not exist */
	unsigned id; /* used when doing traversal with marking (bitarray) */
	
	unsigned short filename_len; /* length of filename including zero term */
	unsigned short depth;	/* depth in the graph. used for priority when buliding */
	
	/* various flags (4 bytes in the end) */
	unsigned dirty:8; /* non-zero if the node has to be rebuilt */
	unsigned depchecked:1; /* set if a dependency checker have processed the file */
	unsigned cached:1;
	unsigned parenthastool:1; /* set if a parent has a tool */
	unsigned counted:1;
	unsigned isdependedon:1; /* set if someone depends on this node */
	
	volatile unsigned workstatus:2; /* 0 = undone, 1 = in the workings, 2 = done*/
};

RB_HEAD(NODERB, NODE);

/* cache node */
struct CACHENODE
{
	RB_ENTRY(CACHENODE) rbentry;

	unsigned hashid;
	time_t timestamp;
	char *filename;
	
	unsigned cmdhash;
	
	unsigned deps_num;
	unsigned *deps; /* index id, not hashid */
};

/* */
struct GRAPH
{
	struct NODERB nodehash[0x10000];
	struct NODE *first;
	struct NODE *last;
	struct HEAP *heap;
	
	/* needed when saving the cache */
	int num_nodes;
	int num_deps;
};

struct HEAP;
struct CONTEXT;

/* node status */
#define NODESTATUS_UNDONE 0
#define NODESTATUS_WORKING 1
#define NODESTATUS_DONE 2

/* node creation error codes */
#define NODECREATE_OK 0
#define NODECREATE_EXISTS 1
#define NODECREATE_NOTNICE 2

/* node walk flags */
#define NODEWALK_FORCE 1
#define NODEWALK_TOPDOWN 2
#define NODEWALK_BOTTOMUP 4
#define NODEWALK_UNDONE 8
#define NODEWALK_QUICK 16

/* node dirty status */
#define NODEDIRTY_NOT 0
#define NODEDIRTY_CMDHASH 1
#define NODEDIRTY_DEPDIRTY 2
#define NODEDIRTY_DEPNEWER 3
#define NODEDIRTY_GLOBALSTAMP 4


/* you destroy graphs by destroying the heap */
struct GRAPH *node_create_graph(struct HEAP *heap);
struct HEAP *node_graph_heap(struct GRAPH *graph);

/* */
int node_create(struct NODE **node, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline);
struct NODE *node_find(struct GRAPH *graph, const char *filename);
struct NODE *node_add_dependency(struct NODE *node, const char *filename);
struct NODE *node_add_dependency_withnode(struct NODE *node, struct NODE *depnode);

struct NODEWALKPATH
{
	struct NODEWALKPATH *parent;
	struct NODE *node;
};

struct NODEWALK
{
	struct NODE *node;
	void *user;
	
	struct NODEWALKPATH *parent;
	
	unsigned depth;
	int flags;
	int (*callback)(struct NODEWALK *);
	unsigned char *mark;
};

int node_walk(
	struct NODE *node,
	int flags,
	int (*callback)(struct NODEWALK *info),
	void *u);

void node_debug_dump(struct GRAPH *graph);
void node_debug_dump_jobs(struct GRAPH *graph);
void node_debug_dump_tree(struct NODE *root);
void node_debug_dump_dot(struct GRAPH *graph, struct NODE *node);

#endif /* FILE_NODE_H */
