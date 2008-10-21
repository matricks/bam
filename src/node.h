
/***** dependency graph *******/

#include <time.h>
#include "tree.h"

/**/
struct DEPENDENCY
{
	struct NODE *node;
	struct DEPENDENCY *next;
};

#define USE_NODE_RB

struct SCANNER
{
	struct SCANNER *next;
	int (*scannerfunc)(struct NODE *, struct SCANNER *info);
};

/**/
struct NODE
{
	/* *** */
	struct GRAPH *graph;
	struct NODE *next;
	struct DEPENDENCY *firstdep; /* list of dependencies */

#ifdef USE_NODE_RB
	RB_ENTRY(NODE) rbentry;
#else
	struct NODE *hashnext;
#endif

	/* filename and the tool to build the resource */
	char *filename; /* this contains the filename with the FULLPATH */
	/*char *tool; */ /* should be an id or something into an array of tools */
	
	char *cmdline; /* command line that should be executed to build this node */
	char *label; /* what to print when we build this node */
	
	struct SCANNER *firstscanner;

	unsigned int hashid;
	
	time_t timestamp;
	unsigned int id; /* is this needed at all? */
	
	/* various flags */
	unsigned int dirty:1; /* set if the node has to be rebuilt */
	unsigned int depchecked:1; /* set if a dependency checker have processed the file */
	unsigned int cached:1;
	unsigned int parenthastool:1; /* set if a parent has a tool */
	unsigned int counted:1;
	
	volatile unsigned int workstatus:2; /* 0 = undone, 1 = in the workings, 2 = done*/
};

struct HEAP;
struct GRAPH;

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

/* you destroy graphs by destroying the heap */
struct GRAPH *node_create_graph(struct HEAP *heap);
struct HEAP *node_graph_heap(struct GRAPH *graph);

int node_create(struct NODE **node, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline);
struct NODE *node_find(struct GRAPH *graph, const char *filename);
struct NODE *node_add_dependency(struct NODE *node, const char *filename);

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
void node_debug_dump_tree(struct NODE *root);
void node_debug_dump_dot(struct GRAPH *graph, struct NODE *node);
