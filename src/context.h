#include <time.h>

struct CONTEXT;

struct DEFERRED
{
	struct DEFERRED *next;
	struct NODE *node;
	int (*run)(struct CONTEXT *context, struct DEFERRED *info);
	void *user;
};

struct CONTEXT
{
	/* lua state */
	struct lua_State *lua;
	
	/* general script information */
	const char *filename;
	char script_directory[512];
	
	struct HEAP *graphheap;
	struct GRAPH *graph;
	struct CACHE *cache;

	/* targets */
	struct NODE *defaulttarget;	/* default target if no targets are specified */
	struct NODE *target;		/* target to build */

	/* list of jobs that we must build */
	struct JOB **joblist;
	unsigned num_jobs;			/* number of jobs in the joblist */
	unsigned first_undone_job;	/* index to first job in the joblist that is undone */
	unsigned current_job_num;	/* current job we are building, not an index, just a count */

	/* this heap is used for dependency lookups that has to happen after we 
		parsed the whole file */
	struct HEAP *deferredheap;
	struct DEFERRED *firstdeferred_cpp;
	struct DEFERRED *firstdeferred_search;
	
	time_t globaltimestamp;		/* timestamp of the script files */
	time_t buildtime;			/* timestamp when the build started */

	int forced;
	int exit_on_error;
	int errorcode;
};

const char *context_get_path(struct lua_State *L);
struct CONTEXT *context_get_pointer(struct lua_State *L);
int context_default_target(struct CONTEXT *context, struct NODE *node);

int context_build_prepare(struct CONTEXT *context);
int context_build_prioritize(struct CONTEXT *context);
int context_build_clean(struct CONTEXT *context);
int context_build_make(struct CONTEXT *context);

extern const char *CONTEXT_LUA_SCRIPTARGS_TABLE;
extern const char *CONTEXT_LUA_TARGETS_TABLE;
extern const char *CONTEXT_LUA_PATH;
extern const char *CONTEXT_LUA_WORKPATH;
