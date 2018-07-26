#ifndef FILE_NODE_H
#define FILE_NODE_H


/* **** dependency graph ****** */

#include <time.h>
#include "tree.h"
#include "support.h"

/* */
struct STRINGLINK
{
	struct STRINGLINK *next;
	char *str;
};

/* */
struct NODELINK
{
	struct NODELINK *next;
	struct NODE *node;
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

struct JOB
{
	struct GRAPH *graph; /* graph that the job belongs to */

	struct JOB *next; /* next job in the global joblist. head is stored in the graph */

	struct NODELINK *firstoutput; /* list of all outputs */
	struct STRINGLINK *firstsideeffect; /* list of all side effects (used for verification) */
	struct STRINGLINK *firstclean; /* list of extra files to remove when cleaning */

	struct NODELINK *firstjobdep; /* list of job dependencies */
	struct NODETREELINK *jobdeproot; /* tree of job dependencies */
	
	struct NODELINK *constraint_exclusive; /* list of exclusive constraints */
	struct NODELINK *constraint_shared; /* list of shared constraints */

	unsigned constraint_exclusive_count; /* */
	unsigned constraint_shared_count; /* */

	char *cmdline;
	char *label;
	char *filter;

	unsigned id; /* unique id */
	
	hash_t cmdhash; /* hash of the command line for detecting changes */
	hash_t cachehash; /* hash that should be written to the cache */

	unsigned priority; /* the priority is the priority of all jobs dependent on this job */

	unsigned counted:1; /* set if we have counted this job towards the number of targets to build */
	unsigned cleaned:1; /* set if we have cleaned this job */

	volatile unsigned status; /* build status of the job, JOBSTATUS_* flags */
};

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

	struct NODE * volatile nextstat; /* next node to stat, written by main-thread, read by stat-thread*/

	struct NODELINK *constraint_exclusive; /* list of exclusive constraints */
	struct NODELINK *constraint_shared; /* list of shared constraints */
	
	struct JOB *job; /* job that produces this node */
	const char *filename; /* this contains the filename with the FULLPATH */

	hash_t hashid; /* hash of the filename/nodename */
	
	/* time stamps, 0 == does not exist. */
	time_t timestamp; /* timestamp. this will be propagated from the deps of the node */
	time_t timestamp_raw; /* raw timestamp. contains the timestamp on the disc */
	
	unsigned id; /* used when doing traversal with marking (bitarray) */
	
	unsigned short filename_len; /* length of filename including zero term */
	
	/* various flags (4 bytes in the end) */
	unsigned dirty:8; /* non-zero if the node has to be rebuilt */
	unsigned depchecked:1; /* set if a dependency checker have processed the file */
	unsigned targeted:1; /* set if this node is targeted for a build */
	unsigned cached:1; /* set if the node should be considered as cached */
};

/* cache node */
struct CACHEINFO_DEPS
{
	RB_ENTRY(CACHEINFO_DEPS) rbentry;

	hash_t hashid;
	time_t timestamp_raw;
	char *filename;

	unsigned cached:1;
	
	unsigned deps_num;
	unsigned *deps; /* index id, not hashid */
};

/* */
struct CACHEINFO_OUTPUT
{
	hash_t hashid;
	hash_t cmdhash;
	time_t timestamp;
};


/* */
struct GRAPH
{
	/* nodes */
	struct NODETREELINK *nodehash[0x10000];
	struct NODE *first;
	struct NODE *last;

	/* jobs */
	struct JOB *firstjob;

	/* file stating */
	void *statthread;
	struct NODE * volatile firststatnode; /* first node that we should stat, written by main-thread, read by stat-thread */
	struct NODE * volatile finalstatnode; /* the very last node that we should stat, written by main-thread, read by stat-thread */
	struct NODE *laststatnode; /* last node stats to, only read and written by the main-thread */

	/* memory */
	struct HEAP *heap;

	/* needed when saving the cache */
	int num_nodes;
	int num_jobs; /* only real jobs */
	int num_deps;
};

struct HEAP;
struct CONTEXT;

/* job status */
#define JOBSTATUS_UNDONE	0	/* node needs build */
#define JOBSTATUS_WORKING	1	/* a thread is working on this node */
#define JOBSTATUS_DONE		2	/* node built successfully */
#define JOBSTATUS_BROKEN	3	/* node tool reported an error or a dependency is broken */

/* special defines */
#define TIMESTAMP_NONE		-1
#define TIMESTAMP_PSEUDO	1

/* node creation error codes */
#define NODECREATE_OK			0
#define NODECREATE_EXISTS		1	/* the node already exists */
#define NODECREATE_NOTNICE		2	/* the path is not normalized */
#define NODECREATE_INVALID_ARG	3	/* invalid arguments */

/* node walk flags */
#define NODEWALK_FORCE		1	/* skips dirty checks */
#define NODEWALK_NOABORT	2	/* continues even if an error have been return by the callback */
#define NODEWALK_TOPDOWN	4	/* callbacks are done top down */
#define NODEWALK_BOTTOMUP	8	/* callbacks are done bottom up */
#define NODEWALK_UNDONE		16	/* causes checking of the undone flag, does not decend if it's set */
#define NODEWALK_QUICK		32	/* never visit the same node twice */
#define NODEWALK_JOBS		64	/* walk the jobtree instead of the complete tree */
#define NODEWALK_REVISIT	(128|NODEWALK_QUICK) /* will do a quick pass and revisits all nodes thats
	have been marked by node_walk_revisit(). path info won't be available when revisiting nodes */

/* node dirty status */
/* make sure to update node_debug_dump_jobs() when changing these */
#define NODEDIRTY_MISSING		1	/* the output file is missing */
#define NODEDIRTY_CMDHASH		2	/* the command doesn't match the one in the cache */
#define NODEDIRTY_DEPDIRTY		4	/* one of the dependencies is dirty */
#define NODEDIRTY_DEPNEWER		8	/* one of the dependencies is newer */
#define NODEDIRTY_GLOBALSTAMP	16	/* the globaltimestamp is newer */
#define NODEDIRTY_FORCED		32	/* forced dirty */
#define NODEDIRTY_NUMFLAGS		6	/* last flag */

/* you destroy graphs by destroying the heap */
struct GRAPH *node_graph_create(struct HEAP *heap);
void node_graph_start_statthread(struct GRAPH *graph);
void node_graph_end_statthread(struct GRAPH *graph);

/* node jobs */
struct JOB *node_job_create_null(struct GRAPH *graph);
struct JOB *node_job_create(struct GRAPH *graph, const char *label, const char *cmdline);
struct NODE *node_job_add_dependency(struct NODE *node, struct NODE *depnode);

/* */
int node_create(struct NODE **node, struct GRAPH *graph, const char *filename, struct JOB *job, time_t timestamp);
struct NODE *node_find(struct GRAPH *graph, const char *filename);
struct NODE *node_find_byhash(struct GRAPH *graph, hash_t hashid);
struct NODE *node_get(struct GRAPH *graph, const char *filename);
struct NODE *node_add_dependency(struct NODE *node, struct NODE *depnode);
void node_cached(struct NODE *node);

/* */
int node_add_sideeffect(struct NODE *node, const char * filename);
int node_add_clean(struct NODE *node, const char * filename);

/* */
struct NODE *node_add_constraint_shared(struct NODE *node, struct NODE *contraint);
struct NODE *node_add_constraint_exclusive(struct NODE *node, struct NODE *contraint);


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
void node_debug_dump(struct GRAPH *graph, int html);

#endif /* FILE_NODE_H */
