#ifndef FILE_NODE_H
#define FILE_NODE_H


/* **** dependency graph ****** */

#include <time.h>
#include "tree.h"
#include "support.h"

/* */
struct NODELINK
{
	struct NODE *node;
	struct NODELINK *next;
};

struct NODETREELINK
{
	struct NODETREELINK *parent;
	struct NODETREELINK *leafs[2];
	struct NODE *node;
	int depth;
};

struct SCANNER
{
	struct SCANNER *next;
	int (*scannerfunc)(struct NODE *, struct SCANNER *info);
};

#if 0
struct JOB
{
	struct GRAPH *graph; /* graph that the job belongs to */
	
	char *cmdline;
	char *label;
	char *filter;

	hash_t cmdhash; /* hash of the command line for detecting changes */
	hash_t cachehash; /* hash that should be written to the cache */

	struct NODELINK *firstoutput;
	
	struct NODELINK *constraint_exclusive; /* list of exclusive constraints */
	struct NODELINK *constraint_shared; /* list of shared constraints */
	
};
#endif

/*
	a node in the dependency graph
	NOTE: when adding variables to this structure, they will all be set
		to zero when created by node_create().
	TODO: these should be allocated cache aligned, and padded to 128 byte?
*/
struct NODE
{
	/* *** */
	struct GRAPH *graph; /* graph that the node belongs to */
	struct NODE *next; /* next node in the graph */
	struct NODELINK *firstparent; /* list of parents */
	
	struct NODELINK *firstdep; /* list of dependencies */
	struct NODETREELINK *deproot; /* tree of dependencies */
	
	struct NODELINK *firstjobdep; /* list of job dependencies */
	struct NODETREELINK *jobdeproot; /* tree of job dependencies */

	struct NODELINK *constraint_exclusive; /* list of exclusive constraints */
	struct NODELINK *constraint_shared; /* list of shared constraints */
	
	char *filename; /* this contains the filename with the FULLPATH */
	
	/* either none of these are set or both of em are */
	char *cmdline; /* command line that should be executed to build this node */
	char *label; /* what to print when we build this node */
	
	char *filter; /* filter string, first character sets the type of filter */
	
	/* filename and the tool to build the resource */
	hash_t cmdhash; /* hash of the command line for detecting changes */
	hash_t cachehash; /* hash that should be written to the cache */
	 
	unsigned constraint_exclusive_count; /* */
	unsigned constraint_shared_count; /* */

	hash_t hashid; /* hash of the filename/nodename */
	
	/* time stamps, 0 == does not exist. */
	time_t timestamp; /* timestamp. this will be updated from the deps of the node */
	time_t timestamp_raw; /* raw timestamp. contains the timestamp on the disc */
	
	unsigned id; /* used when doing traversal with marking (bitarray) */
	
	unsigned short filename_len; /* length of filename including zero term */
	unsigned short depth;	/* depth in the graph. used for priority when buliding */
	
	/* various flags (4 bytes in the end) */
	unsigned dirty:8; /* non-zero if the node has to be rebuilt */
	unsigned depchecked:1; /* set if a dependency checker have processed the file */
	unsigned counted:1; /* set if we have counted this node towards the number of targets to build */
	unsigned targeted:1; /* set if this node is targeted for a build */
	unsigned touch:1; /* when built, touch the output file as well */
	unsigned cached:1; /* set if the node should be considered as cached */
	
	volatile unsigned workstatus:2; /* build status of the node, NODESTATUS_* flags */
};

/* cache node */
struct CACHENODE
{
	RB_ENTRY(CACHENODE) rbentry;

	hash_t hashid;
	time_t timestamp_raw;
	char *filename;
	hash_t cmdhash;

	unsigned cached:1;
	
	unsigned deps_num;
	unsigned *deps; /* index id, not hashid */
};

/* */
struct GRAPH
{
	/* nodes */
	struct NODETREELINK *nodehash[0x10000];
	struct NODE *first;
	struct NODE *last;

	/* memory */
	struct HEAP *heap;

	/* needed when saving the cache */
	int num_nodes;
	int num_deps;
};

struct HEAP;
struct CONTEXT;

/* node status */
#define NODESTATUS_UNDONE 0   /* node needs build */
#define NODESTATUS_WORKING 1  /* a thread is working on this node */
#define NODESTATUS_DONE 2     /* node built successfully */
#define NODESTATUS_BROKEN 3   /* node tool reported an error or a dependency is broken */

/* node creation error codes */
#define NODECREATE_OK 0
#define NODECREATE_EXISTS 1  /* the node already exists */
#define NODECREATE_NOTNICE 2 /* the path is not normalized */
#define NODECREATE_INVALID_ARG 3 /* invalid arguments */

/* node walk flags */
#define NODEWALK_FORCE 1    /* skips dirty checks and*/
#define NODEWALK_TOPDOWN 2  /* callbacks are done top down */
#define NODEWALK_BOTTOMUP 4 /* callbacks are done bottom up */
#define NODEWALK_UNDONE 8   /* causes checking of the undone flag, does not decend if it's set */
#define NODEWALK_QUICK 16   /* never visit the same node twice */
#define NODEWALK_JOBS 32   /* walk the jobtree instead of the complete tree */
#define NODEWALK_REVISIT (64|NODEWALK_QUICK) /* will do a quick pass and revisits all nodes thats
	have been marked by node_walk_revisit(). path info won't be available when revisiting nodes */

/* node dirty status */
/* make sure to update node_debug_dump_jobs() when changing these */
#define NODEDIRTY_NOT 0
#define NODEDIRTY_MISSING 1     /* the output file is missing */
#define NODEDIRTY_CMDHASH 2     /* the command doesn't match the one in the cache */
#define NODEDIRTY_DEPDIRTY 3    /* one of the dependencies is dirty */
#define NODEDIRTY_DEPNEWER 4    /* one of the dependencies is newer */
#define NODEDIRTY_GLOBALSTAMP 5 /* the globaltimestamp is newer */

/* you destroy graphs by destroying the heap */
struct GRAPH *node_create_graph(struct HEAP *heap);

/* */
int node_create(struct NODE **node, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline);
struct NODE *node_find(struct GRAPH *graph, const char *filename);
struct NODE *node_find_byhash(struct GRAPH *graph, hash_t hashid);
struct NODE *node_get(struct GRAPH *graph, const char *filename);
struct NODE *node_add_dependency(struct NODE *node, const char *filename);
struct NODE *node_add_dependency_withnode(struct NODE *node, struct NODE *depnode);
struct NODE *node_add_job_dependency_withnode(struct NODE *node, struct NODE *depnode);
void node_set_pseudo(struct NODE *node);
void node_cached(struct NODE *node);

struct NODE *node_add_constraint_shared(struct NODE *node, const char *filename);
struct NODE *node_add_constraint_exclusive(struct NODE *node, const char *filename);

struct NODEWALKPATH
{
	struct NODEWALKPATH *parent;
	struct NODE *node;
};

struct NODEWALKREVISIT
{
	struct NODE *node;
	struct NODEWALKREVISIT *next;
};

struct NODEWALK
{
	int flags; /* flags for this node walk */
	struct NODE *node; /* current visiting node */

	/* path that we reached this node by (not available during revisit due to activation) */
	struct NODEWALKPATH *parent;
	unsigned depth;
	
	void *user;
	int (*callback)(struct NODEWALK *); /* function that is called for each visited node */
	
	unsigned char *mark; /* bits for mark and sweep */

	int revisiting; /* set to 1 if we are doing revisits */
	struct NODEWALKREVISIT *firstrevisit;
	struct NODEWALKREVISIT *revisits;
};

/* walks though the dependency tree with the set options and calling callback()
	on each node it visites */
int node_walk(
	struct NODE *node,
	int flags,
	int (*callback)(struct NODEWALK *info),
	void *u);

/* marks a node for revisit, only works if NODEWALK_REVISIT flags
	was specified to node_walk */
void node_walk_revisit(struct NODEWALK *walk, struct NODE *node);

/* node debug dump functions */
void node_debug_dump(struct GRAPH *graph);
void node_debug_dump_detailed(struct GRAPH *graph);
void node_debug_dump_jobs(struct GRAPH *graph);
void node_debug_dump_dot(struct GRAPH *graph, struct NODE *top);
void node_debug_dump_jobs_dot(struct GRAPH *graph, struct NODE *top);

#endif /* FILE_NODE_H */
