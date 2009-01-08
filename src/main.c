/* TODO: clean up this file and move functions to their own files */

/* system includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* lua includes */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* UNIX includes. needed? */
#include <sys/types.h>
#include <sys/stat.h>

/* program includes */
#include "mem.h"
#include "node.h"
#include "path.h"
#include "support.h"
#include "dep_cpp.h"
#include "context.h"
#include "platform.h"
#include "version.h"

/* internal base.bam file */
#include "internal_base.h"

/* why is this needed? -kma
   because of getcwd() -jmb */
#ifdef BAM_FAMILY_UNIX
#include <unistd.h>
#endif
#ifdef BAM_FAMILY_BEOS
#include <unistd.h>
#endif
#ifdef BAM_FAMILY_WINDOWS
#include <direct.h>
#define getcwd _getcwd /* stupid msvc is calling getcwd non-ISO-C++ conformant */
#endif

#define DEFAULT_REPORT_STYLE "s"

/* ** */
static const char *program_name = "bam";
#define L_FUNCTION_PREFIX "bam_"

struct OPTION
{
	const char **s;
	int *v;
	const char *sw;
	const char *desc;
};

/* options passed via the command line */
static int option_clean = 0;
static int option_verbose = 0;
static int option_dry = 0;
static int option_simpleoutput = 0;
static int option_threads = 1;
static int option_debug_nodes = 0;
static int option_debug_buildtime = 0;
static int option_debug_dirty = 0;
static int option_debug_dumpinternal = 0;
static int option_print_help = 0;
static const char *option_script = "default.bam"; /* -f filename */
static const char *option_basescript = 0x0; /* -b filename or BAM_BASE */
static const char *option_threads_str = "0";
static const char *option_report_str = DEFAULT_REPORT_STYLE;
static const char *option_targets[128] = {0};
static int option_num_targets = 0;

static const char *option_scriptargs[128] = {0};
static int option_num_scriptargs = 0;

static int option_report_color = 0;
static int option_report_bar = 0;
static int option_report_steps = 0;

static struct OPTION options[] = {
	/*@OPTION Targets ( name )
		Specify a target to be built. A target can be any output
		specified to the [AddJob] function.
		
		If no targets are specified, the default target will be built
		If there are no default target and there is only one target
		specified with the [Target] function, it will be built.
		Otherwise bam will report an error.
		
		There is a special pseudo target named ^all^ that represents
		all targets specified by the [Target] function.
	@END*/

	/*@OPTION Script Arguments ( name=value )
		TODO
	@END*/

	/*@OPTION Base File ( -b FILENAME )
		Base file to use. In normal operation, Bam executes the builtin
		^base.bam^ during startup before executing the build script
		itself. This option allows to control that behaviour and
		loading your own base file if wanted.
	@END*/
	{&option_basescript,0		, "-b", "base file to use"},

	/*@OPTION Script File ( -s FILENAME )
		Bam file to use. In normal operation, Bam executes
		^default.bam^. This option allows you to specify another bam
		file.
	@END*/
	{&option_script,0			, "-s", "bam file to use"},

	/*@OPTION Clean ( -c )
		Cleans the specified targets or the default target.
	@END*/
	{0, &option_clean			, "-c", "clean"},

	/*@OPTION Verbose ( -v )
		Prints all commands that are runned when building.
	@END*/
	{0, &option_verbose			, "-v", "verbose"},

	/*@OPTION Threading ( -j N )
		Sets the number of threads used when building.
		Set to 0 to disable.
	@END*/
	{&option_threads_str,0		, "-j", "sets the number of threads to use. 0 disables threading. (EXPRIMENTAL)"},

	/*@OPTION Help ( -h, --help )
		Prints out a short reference of the command line options and quits
		directly after.
	@END*/
	{0, &option_print_help		, "-h", "prints this help"},
	{0, &option_print_help		, "--help", "prints this help"},

	/*@OPTION Report Format ( -r [b][s][c] )
		Sets the format of the progress report when building.
		<ul>
			<li>b</li> - Use a progress bar showing the precenage.
			<li>s</li> - Show each step when building. (default)
			<li>c</li> - Use ANSI colors.
		</ul>
	@END*/
	{&option_report_str,0		, "-r", "sets report format, b = bar, s = steps, c = color, (default:\"" DEFAULT_REPORT_STYLE "\")"},

	/*@OPTION Dry Run ( --dry )
		Does everything that it normally would do but does not execute any
		commands.
	@END*/
	{0, &option_dry				, "--dry", "dry run"},

	/*@OPTION Debug: Build Time ( --debug-build-time )
		Prints out the time spent building the targets when done.
	@END*/
	{0, &option_debug_buildtime	, "--debug-build-time", "prints the build time"},

	/*@OPTION Debug: Dump Nodes ( --debug-nodes )
		Dumps all nodes in the dependency graph, their state and their
		dependent nodes. This is useful if you are writing your own
		actions to verify that dependencies are correctly added.
	@END*/
	{0, &option_debug_nodes		, "--debug-nodes", "prints all the nodes with dependencies"},

	/*@OPTION Debug: Dirty Marking ( --debug-dirty )
	@END*/
	{0, &option_debug_dirty		, "--debug-dirty", ""},
	
	/*@OPTION Debug: Dump Internal Scripts ( --debug-dump-internal )
	@END*/
	{0, &option_debug_dumpinternal		, "--debug-dump-internal", "dumps the internals scripts to stdout"},
	
	
	/* terminate list */
	{0, 0, (const char*)0, (const char*)0}
};

/****/
void debug_print_lua_value(lua_State *L, int i)
{
	if(lua_type(L,i) == LUA_TNIL)
		printf("nil");
	else if(lua_type(L,i) == LUA_TSTRING)
		printf("'%s'", lua_tostring(L,i));
	else if(lua_type(L,i) == LUA_TNUMBER)
		printf("%f", lua_tonumber(L,i));
	else if(lua_type(L,i) == LUA_TBOOLEAN)
	{
		if(lua_toboolean(L,i))
			printf("true");
		else
			printf("false");
	}
	else if(lua_type(L,i) == LUA_TTABLE)
	{
		printf("{...}");
	}
	else
		printf("%p (%s (%d))", lua_topointer(L,i), lua_typename(L,lua_type(L,i)), lua_type(L,i));
}

/* error function */
static int lf_errorfunc(lua_State *L)
{
	int depth = 0;
	int frameskip = 1;
	lua_Debug frame;

	if(option_report_color)
		printf("\033[01;31m%s\033[00m\n", lua_tostring(L,-1));
	else
		printf("%s\n", lua_tostring(L,-1));
	
	printf("stack traceback:\n");
	
	while(lua_getstack(L, depth, &frame) == 1)
	{
		depth++;
		
		lua_getinfo(L, "nlSf", &frame);

		/* check for functions that just report errors. these frames just confuses more then they help */
		if(frameskip && strcmp(frame.short_src, "[C]") == 0 && frame.currentline == -1)
			continue;
		frameskip = 0;
		
		/* print stack frame */
		printf("  %s(%d): %s %s\n", frame.short_src, frame.currentline, frame.name, frame.namewhat);
		
		/* print all local variables for the frame */
		{
			int i;
			const char *name = 0;
			
			i = 1;
			while((name = lua_getlocal(L, &frame, i)) != NULL)
			{
				printf("    %s = ", name);
				debug_print_lua_value(L,-1);
				printf("\n");
				lua_pop(L,1);
				i++;
			}
			
			i = 1;
			while((name = lua_getupvalue(L, -1, i)) != NULL)
			{
				/* TODO: parse value as well*/
				printf("    upvalue: %d %s\n", i-1, name);
				lua_pop(L,1);
				i++;
			}
		}
	}
	return 1;
}

/*
	add_job(string output, string label, string command)
*/
static int lf_add_job(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;
	int n = lua_gettop(L);
	int i;
	if(n != 3)
	{
		lua_pushstring(L, "add_job: incorrect number of arguments");
		lua_error(L);
	}
	
	/* fetch contexst from lua */
	context = context_get_pointer(L);

	/* create the node */
	i = node_create(&node, context->graph, lua_tostring(L,1), lua_tostring(L,2), lua_tostring(L,3));
	if(i == NODECREATE_NOTNICE)
	{
		printf("%s: '%s' is not nice\n", program_name, lua_tostring(L,2));
		lua_pushstring(L, "add_job: path is not nice");
		lua_error(L);
	}
	else if(i == NODECREATE_EXISTS)
	{
		printf("%s: '%s' already exists\n", program_name, lua_tostring(L,2));
		lua_pushstring(L, "add_job: node already exists");
		lua_error(L);
	}
	else if(i != NODECREATE_OK)
	{
		lua_pushstring(L, "add_job: unknown error creating node");
		lua_error(L);
	}
	
	return 0;
}

/* ********** */
static int lf_add_dependency(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;

	int n = lua_gettop(L);
	int i;
	
	if(n < 2)
	{
		lua_pushstring(L, "add_dep: to few arguments");
		lua_error(L);
	}

	context = context_get_pointer(L);

	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
	{
		char buf[512];
		sprintf(buf, "add_dep: couldn't find node with name '%s'", lua_tostring(L,1));
		lua_pushstring(L, buf);
		lua_error(L);
	}
	
	/* seek deps */
	for(i = 2; i <= n; ++i)
	{
		if(lua_isstring(L,n))
		{
			if(!node_add_dependency(node, lua_tostring(L,n)))
			{
				lua_pushstring(L, "add_dep: could not add dependency");
				lua_error(L);
			}
		}
		else
		{
			lua_pushstring(L, "add_dep: dependency is not a string");
			lua_error(L);
		}
	}
	
	return 0;
}

/* default_target(string filename) */
static int lf_default_target(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;

	int n = lua_gettop(L);
	if(n != 1)
	{
		lua_pushstring(L, "default_target: incorrect number of arguments");
		lua_error(L);
	}
	
	if(!lua_isstring(L,1))
	{
		lua_pushstring(L, "default_target: expected string");
		lua_error(L);
	}

	/* fetch context from lua */
	context = context_get_pointer(L);

	/* search for the node */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
	{
		lua_pushstring(L, "default_target: node not found");
		lua_error(L);
	}
	
	/* set target */
	context_default_target(context, node);
	return 0;
}

/* add_target(string filename) */
static int lf_add_target(lua_State *L)
{
	struct NODE *node;
	struct CONTEXT *context;

	int n = lua_gettop(L);
	if(n != 1)
	{
		lua_pushstring(L, "add_target: incorrect number of arguments");
		lua_error(L);
	}
	
	if(!lua_isstring(L,1))
	{
		lua_pushstring(L, "add_target: expected string");
		lua_error(L);
	}

	/* fetch context from lua */
	context = context_get_pointer(L);

	/* search for the node */
	node = node_find(context->graph, lua_tostring(L,1));
	if(!node)
	{
		lua_pushstring(L, "add_target: node not found");
		lua_error(L);
	}
	
	/* add target */
	if(context_add_target(context, node))
	{
		lua_pushstring(L, "add_target: target already exists");
		lua_error(L);
	}
	
	/* we need to save the options some where */
	return 0;
}

/* update_globalstamp(string filename) */
static int lf_update_globalstamp(lua_State *L)
{
	struct CONTEXT *context;

	int n = lua_gettop(L);
	time_t file_stamp;
	
	if(n < 1)
	{
		lua_pushstring(L, "update_globalstamp: to few arguments");
		lua_error(L);
	}

	context = context_get_pointer(L);
	file_stamp = file_timestamp(lua_tostring(L,1));
	
	if(file_stamp > context->globaltimestamp)
		context->globaltimestamp = file_stamp;
	
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
	int count = (context->current_cmd_num*max)/context->num_commands;
	int precent = (context->current_cmd_num*100)/context->num_commands;

	if(option_report_color)
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

static int run_node(struct CONTEXT *context, struct NODE *node, int thread_id)
{
	int ret;
	
	if(node->label && node->label[0])
	{
		context->current_cmd_num++;
		
		if(1)
		{
			static const char *format = 0;
			if(!format)
			{
				static char buf[64];
				int num = 0;
				int c = context->num_commands;
				for(; c; c /= 10)
					num++;
				
				if(option_report_color)
					sprintf(buf, "\033[01;32m[%%%dd/%%%dd] \033[01;36m#%%d\033[00m %%s\n", num, num);
				else
					sprintf(buf, "[%%%dd/%%%dd] #%%d %%s\n", num, num);
				format = buf;
			}
			
			if(option_report_bar)
				progressbar_clear();
			if(option_report_steps)
			{
				if(option_simpleoutput)
					printf("%s", node->label);
				else
					printf(format, context->current_cmd_num, context->num_commands, thread_id, node->label);
			}
		}
		
		if(option_report_bar)
			progressbar_draw(context);
	}
	
	if(option_verbose)
	{
		if(option_report_color)
			printf("\033[01;33m%s\033[00m\n", node->cmdline);
		else
			printf("%s\n", node->cmdline);
	}
		
	fflush(stdout);
	
	/* execute the command */
	criticalsection_leave();
	ret = system(node->cmdline);
	criticalsection_enter();
	
	if(ret)
	{
		if(option_report_color)
			printf("\033[01;31m%s: command returned error %d\033[00m\n", program_name, ret);
		else
			printf("%s: command returned error: %d\n", program_name, ret);
		fflush(stdout);
	}
	return ret;
}

struct THREADINFO
{
	int id;
	struct CONTEXT *context;
	struct NODE *node; /* the top node */
	int errorcode;
};

static int threads_run_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct THREADINFO *info = (struct THREADINFO *)walkinfo->user;
	struct DEPENDENCY *dep;
	int errorcode = 0;

	/* make sure that all deps are done */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(dep->node->dirty && dep->node->workstatus != NODESTATUS_DONE)
		/*if(dep->node->workstatus != NODESTATUS_DONE)*/
			return 0;
	}

	/* if it doesn't have a tool, just mark it as done and continue the search */
	if(!node->cmdline)
	{
		node->workstatus = NODESTATUS_DONE;
		return 0;
	}
	
	/* mark the node as its in the working */
	node->workstatus = NODESTATUS_WORKING;
	
	/* run the node */
	if(node->cmdline)
		errorcode = run_node(info->context, node, info->id+1);
	
	/* this node is done, mark it so and return the error code */
	node->workstatus = NODESTATUS_DONE;
	return errorcode;
}

volatile int nothing_done_counter = 0;

static void threads_run(void *u)
{
	struct THREADINFO *info = (struct THREADINFO *)u;
	int flags = NODEWALK_BOTTOMUP|NODEWALK_UNDONE|NODEWALK_QUICK;
	
	info->errorcode = 0;
	
	/* lock the dependency graph */
	criticalsection_enter();
	
	if(info->node->dirty)
	{
		while(1)
		{
			info->errorcode = node_walk(info->node, flags, threads_run_callback, info);
			
			/* check if we are done */
			if(info->node->workstatus == NODESTATUS_DONE || info->errorcode != 0)
				break;
				
			/* let the others have some time */
			criticalsection_leave();
			threads_yield();
			criticalsection_enter();
		}
	}
	
	criticalsection_leave();
}

static int run(struct CONTEXT *context, struct NODE *node)
{
	/* multithreaded */
	struct THREADINFO info[64];
	void *threads[64];
	int i;
	
	/* clamp number of threads */
	if(option_threads > 64) option_threads = 64;
	else if(option_threads < 1) option_threads = 1;
	
	for(i = 0; i < option_threads; i++)
	{
		info[i].context = context;
		info[i].node = node;
		info[i].id = i;
		info[i].errorcode = 0;
	}

	if(option_threads <= 1)
	{
		/* no threading, use this thread then */
		threads_run(&info[0]);
		if(option_report_bar)
			progressbar_clear();
		return info[0].errorcode;
	}
	else
	{
		/* start threads */
		for(i = 0; i < option_threads; i++)
			threads[i] = threads_create(threads_run, &info[i]);
		
		/* wait for threads */
		for(i = 0; i < option_threads; i++)
			threads_join(threads[i]);
			
		if(option_report_bar)
			progressbar_clear();

		/* check for error codes */		
		for(i = 0; i < option_threads; i++)
		{
			if(info[i].errorcode)
				return info[i].errorcode;
		}
	}
	return 0;
}

static int clean_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	
	/* no tool, no processing */
	if(!node->cmdline)
		return 0;

	if(node->timestamp)
	{
		if(remove(node->filename) == 0)
			printf("%s: removed '%s'\n", program_name, node->filename);
	}
	return 0;
}

static void clean(struct NODE *node)
{
	node_walk(node, NODEWALK_BOTTOMUP|NODEWALK_FORCE, clean_callback, 0);
}

static int count_jobs_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct CONTEXT *context = (struct CONTEXT *)walkinfo->user;
	if(node->cmdline && !node->counted)
	{
		node->counted = 1;
		context->num_commands++;
	}
	return 0;
}

static int count_jobs(struct CONTEXT *context, struct NODE *node)
{
	node_walk(node, NODEWALK_BOTTOMUP|NODEWALK_QUICK, count_jobs_callback, context);
	return 0;
}

static int dirty_mark_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct CONTEXT *context = (struct CONTEXT *)walkinfo->user;
	struct DEPENDENCY *dep;
	time_t time = timestamp();
	
	if(option_debug_dirty)
		printf("%s: dirty: check on '%s'\n", program_name, node->filename);

	/* check against the global timestamp first */
	if(node->cmdline && (!node->timestamp || node->timestamp < context->globaltimestamp))
	{
		if(option_debug_dirty)
		{
			if(!node->timestamp)
				printf("%s: dirty: \ttimestamp == 0\n", program_name);
			else
				printf("%s: dirty: \ttimestamp < globaltimestamp\n", program_name);
		}
		node->dirty = 1;
		return 0;
	}
	
	/* check against all the dependencies */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(node->timestamp > time)
			printf("%s: WARNING:'%s' comes from the future\n", program_name, node->filename);
		
		if(dep->node->dirty)
		{
			if(option_debug_dirty)
				printf("%s: dirty: \tdep '%s' dirty\n", program_name, dep->node->filename);
			node->dirty = 1;
			break;
		}
		else if(node->timestamp < dep->node->timestamp)
		{
			if(node->cmdline)
			{
				if(option_debug_dirty)
					printf("%s: dirty: \tdep '%s' timestamp\n", program_name, dep->node->filename);
				node->dirty = 1;
				break;
			}
			else /* no cmdline, just propagate the timestamp */
			{
				if(option_debug_dirty)
					printf("%s: dirty: \ttimestamp taken from '%s'\n", program_name, dep->node->filename);
				node->timestamp = dep->node->timestamp;
			}
		}
	}
	return 0;
}

int dirty_mark(struct CONTEXT *context, struct NODE *target)
{
	/* TODO: we should have NODEWALK_QUICK here, but then it will fail on circular dependencies */
	return node_walk(target, NODEWALK_BOTTOMUP|NODEWALK_FORCE, dirty_mark_callback, context);
}

static int validate_graph_callback(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct DEPENDENCY *dep;
	struct NODEWALKPATH *path;

	for(dep = node->firstdep; dep; dep = dep->next)
	{
		if(!dep->node->cmdline)
			continue;
			
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
	}
	
	return 0;
}

static int validate_target(struct NODE *target)
{
	return node_walk(target, NODEWALK_BOTTOMUP|NODEWALK_FORCE, validate_graph_callback, 0x0);
}

/* do targets */
static int do_target_pass_setup(struct CONTEXT *context, struct NODE *target)
{
	/* validate and dirty mark */
	/* TODO: these could perhaps be merged to one pass */
	int error;
	
	error = validate_target(target);
	if(error)
		return error;
		
	error = dirty_mark(context, target);
	if(error)
		return error;
		
	count_jobs(context, target);
	return 0;
}

static int do_target_pass_build(struct CONTEXT *context, struct NODE *target)
{
	if(option_clean)
	{
		printf("%s: cleaning '%s'\n", program_name, path_filename(target->filename));
		clean(target);
		return 0;
	}
	else
	{
		printf("%s: building '%s'\n", program_name, path_filename(target->filename));
		return run(context, target);
	}
}

static int do_targets_all(struct CONTEXT *context)
{
	int error;
	struct TARGET *target;
	
	if(!context->firsttarget)
	{
		printf("%s: no targets specified\n", program_name);
		return -1;
	}
	
	for(target = context->firsttarget; target; target = target->next)
	{
		error = do_target_pass_setup(context, target->node);
		if(error)
			return error;
	}
	
	for(target = context->firsttarget; target; target = target->next)
	{
		error = do_target_pass_build(context, target->node);
		if(error)
			return error;
	}
	
	return 0;
}

static int do_targets_specified(struct CONTEXT *context, const char **targets, int num_targets)
{
	int i, error;
	for(i = 0; i < num_targets; i++)
	{
		struct NODE *node = node_find(context->graph, targets[i]);
		if(!node)
		{
			printf("%s: no target named '%s'\n", program_name, targets[i]);
			return -1;
		}
		
		error = do_target_pass_setup(context, node);
		if(error)
			return error;
	}

	for(i = 0; i < num_targets; i++)
	{
		struct NODE *node = node_find(context->graph, targets[i]);
		error = do_target_pass_build(context, node);
		if(error)
			return error;
	}

	return 0;
}

static int do_targets_default(struct CONTEXT *context)
{
	int error;
	if(!context->defaulttarget)
	{
		printf("%s: no default target\n", program_name);
		return -1;
	}
					
	error = do_target_pass_setup(context, context->defaulttarget);
	if(error)
		return error;
	return do_target_pass_build(context, context->defaulttarget);
}


const char *internal_base_reader(lua_State *L, void *data, size_t *size)
{
	char **p = (char **)data;
	if(!*p)
		return 0;
		
	*size = strlen(*p);
	data = *p;
	*p = 0;
	return data;
}

/* *** */
int register_lua_globals(struct CONTEXT *context)
{
	int i, error = 0;
		
	/* add standard libs */
	luaL_openlibs(context->lua);
	
	/* add specific functions */
	lua_register(context->lua, L_FUNCTION_PREFIX"add_job", lf_add_job);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_dependency", lf_add_dependency);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_target", lf_add_target);
	lua_register(context->lua, L_FUNCTION_PREFIX"default_target", lf_default_target);

	/* path manipulation */
	lua_register(context->lua, L_FUNCTION_PREFIX"path_join", lf_path_join);
	lua_register(context->lua, L_FUNCTION_PREFIX"path_fix", lf_path_fix);
	lua_register(context->lua, L_FUNCTION_PREFIX"path_isnice", lf_path_isnice);
	
	lua_register(context->lua, L_FUNCTION_PREFIX"path_ext", lf_path_ext);
	lua_register(context->lua, L_FUNCTION_PREFIX"path_path", lf_path_path);
	lua_register(context->lua, L_FUNCTION_PREFIX"path_filename", lf_path_filename);

	/* various support functions */
	lua_register(context->lua, L_FUNCTION_PREFIX"collect", lf_collect);
	lua_register(context->lua, L_FUNCTION_PREFIX"collectrecursive", lf_collectrecursive);
	lua_register(context->lua, L_FUNCTION_PREFIX"collectdirs", lf_collectdirs);
	lua_register(context->lua, L_FUNCTION_PREFIX"collectdirsrecursive", lf_collectdirsrecursive);
	
	lua_register(context->lua, L_FUNCTION_PREFIX"listdir", lf_listdir);
	lua_register(context->lua, L_FUNCTION_PREFIX"update_globalstamp", lf_update_globalstamp);

	/* error handling */
	lua_register(context->lua, "errorfunc", lf_errorfunc);
	
	/* dependency accelerator functions */
	lua_register(context->lua, L_FUNCTION_PREFIX"dependency_cpp", lf_dependency_cpp);

	/* create arguments table */
	lua_pushstring(context->lua, CONTEXT_LUA_SCRIPTARGS_TABLE);
	lua_newtable(context->lua);
	for(i = 0; i < option_num_scriptargs; i++)
	{
		const char *separator = option_scriptargs[i];
		while(*separator != '=')
			separator++;
		lua_pushlstring(context->lua, option_scriptargs[i], separator-option_scriptargs[i]);
		lua_pushstring(context->lua, separator+1);
		lua_settable(context->lua, -3);
	}
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* create targets table */
	lua_pushstring(context->lua, CONTEXT_LUA_TARGETS_TABLE);
	lua_newtable(context->lua);
	for(i = 0; i < option_num_targets; i++)
	{
		lua_pushstring(context->lua, option_targets[i]);
		lua_rawseti(context->lua, -2, i);
	}
	lua_settable(context->lua, LUA_GLOBALSINDEX);
	
	/* set paths */
	{
		char cwd[512];
		getcwd(cwd, 512);
		
		lua_pushstring(context->lua, CONTEXT_LUA_PATH);
		lua_pushstring(context->lua, context->script_directory);
		lua_settable(context->lua, LUA_GLOBALSINDEX);

		lua_pushstring(context->lua, CONTEXT_LUA_WORKPATH);
		lua_pushstring(context->lua, cwd);
		lua_settable(context->lua, LUA_GLOBALSINDEX);
	}

	/* set context */
	lua_pushstring(context->lua, CONTEXT_LUA_CONTEXT_POINTER);
	lua_pushlightuserdata(context->lua, context);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* set version */
	lua_pushstring(context->lua, "_bam_version");
	lua_pushstring(context->lua, BAM_VERSION_STRING);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	lua_pushstring(context->lua, "_bam_version_complete");
	lua_pushstring(context->lua, BAM_VERSION_STRING_COMPLETE);
	lua_settable(context->lua, LUA_GLOBALSINDEX);
	
	/* set family */
	lua_pushstring(context->lua, "family");
	lua_pushstring(context->lua, BAM_FAMILY_STRING);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* set platform */
	lua_pushstring(context->lua, "platform");
	lua_pushstring(context->lua, BAM_PLATFORM_STRING);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* set arch */
	lua_pushstring(context->lua, "arch");
	lua_pushstring(context->lua, BAM_ARCH_STRING);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* set verbosity level */
	lua_pushstring(context->lua, "verbose");
	lua_pushnumber(context->lua, option_verbose);
	lua_settable(context->lua, LUA_GLOBALSINDEX);

	/* load base script */
	if(option_basescript)
	{
		
		int ret;
		lua_getglobal(context->lua, "errorfunc");

		if(option_verbose)
			printf("%s: reading base script from '%s'\n", program_name, option_basescript);
		
		/* push error function to stack */
		ret = luaL_loadfile(context->lua, option_basescript);
		if(ret != 0)
		{
			if(ret == LUA_ERRSYNTAX)
				printf("%s: syntax error\n", program_name);
			else if(ret == LUA_ERRMEM)
				printf("%s: memory allocation error\n", program_name);
			else if(ret == LUA_ERRFILE)
				printf("%s: error opening '%s'\n", program_name, option_basescript);
			lf_errorfunc(context->lua);
			error = 1;
		}
		else if(lua_pcall(context->lua, 0, LUA_MULTRET, -2) != 0)
			error = 1;
	}
	else
	{
		int ret;
		const char *p;
		int f;
		
		for(f = 0; internal_files[f].filename; f++)
		{
			p = internal_files[f].content;
			
			if(option_verbose)
				printf("%s: reading internal file '%s'\n", program_name, internal_files[f].filename);
		
			lua_getglobal(context->lua, "errorfunc");
			
			/* push error function to stack */
			ret = lua_load(context->lua, internal_base_reader, &p, internal_files[f].filename);
			if(ret != 0)
			{
				if(ret == LUA_ERRSYNTAX)
					printf("%s: syntax error\n", program_name);
				else if(ret == LUA_ERRMEM)
					printf("%s: memory allocation error\n", program_name);
				else
					printf("%s: unknown error parsing base script\n", program_name);
				lf_errorfunc(context->lua);
				error = 1;
			}
			else if(lua_pcall(context->lua, 0, LUA_MULTRET, -2) != 0)
				error = 1;
		}
	}
	
	return error;
}


static int lf_panicfunc(lua_State *L)
{
	printf("%s: PANIC!\n", program_name);
	*(int*)0 = 0;
	return 0;
}

/* *** */
static int bam(const char *scriptfile, const char **targets, int num_targets)
{
	struct CONTEXT context;
	int error = 0;
	int i;
	int report_done = 1;

	/* build time */
	time_t starttime  = time(0x0);
		
	/* clean the context structure */
	memset(&context, 0, sizeof(struct CONTEXT));

	/* create memory heap */
	context.heap = mem_create();
	
	/* create graph */
	context.graph = node_create_graph(context.heap);

	/* set filename */
	context.filename = scriptfile;
	context.filename_short = path_filename(scriptfile);
	
	/* set global timestamp to the script file */
	context.globaltimestamp = file_timestamp(scriptfile);
	
	/* fetch script directory */
	{
		char cwd[512];
		char path[512];

		getcwd(cwd, 512);
		if(path_directory(context.filename, path, 512))
		{
			printf("crap error1\n");
			*((int*)0) = 0;
		}
		
		if(path_join(cwd, path, context.script_directory, 512))
		{
			printf("crap error2\n");
			*((int*)0) = 0;
		}
	}
	
	/* create lua context */
	context.lua = luaL_newstate();
	lua_atpanic(context.lua, lf_panicfunc);
	
	/* register all functions */
	if(register_lua_globals(&context) != 0)
		return 1;
	
	/* load script */	
	if(option_verbose)
		printf("%s: reading script from '%s'\n", program_name, scriptfile);

	if(1)
	{
		int ret;
		lua_getglobal(context.lua, "errorfunc");
		
		/* push error function to stack */
		ret = luaL_loadfile(context.lua, scriptfile);
		if(ret != 0)
		{
			if(ret == LUA_ERRSYNTAX)
				printf("%s: syntax error\n", program_name);
			else if(ret == LUA_ERRMEM)
				printf("%s: memory allocation error\n", program_name);
			else if(ret == LUA_ERRFILE)
				printf("%s: error opening '%s'\n", program_name, scriptfile);
			lf_errorfunc(context.lua);
			error = 1;
		}
		else if(lua_pcall(context.lua, 0, LUA_MULTRET, -2) != 0)
			error = 1;
	}

	/* debug */
	if(option_debug_nodes)
	{
		/* debug dump all nodes */
		node_debug_dump(context.graph);
		report_done = 0;
	}
	else if(option_dry)
	{
		/* do NADA */
	}
	else
	{
		/* run */
		if(error == 0)
		{
			if(num_targets)
			{
				/* search for all target */
				for(i = 0; i < num_targets; i++)
				{
					if(strcmp(targets[i], "all") == 0)
					{
						error = do_targets_all(&context);
						break;
					}
				}
				
				/* no all target was found */
				if(i == num_targets)
					error = do_targets_specified(&context, targets, num_targets);
			}
			else
			{
				/* just do the default target */
				error = do_targets_default(&context);
			}
		}
	}

	/* clean up */
	lua_close(context.lua);
	mem_destroy(context.heap);

	/* print and return */
	if(error)
		printf("%s: error during build\n", program_name);
	else if(report_done)
		printf("%s: done\n", program_name);

	/* print build time */
	if(option_debug_buildtime)
	{
		time_t s = time(0x0) - starttime;
		printf("\nbuild time: %d:%.2d\n", (int)(s/60), (int)(s%60));
	}

	return error;
}


/* */
static void print_help()
{
	int j;
	printf("bam version " BAM_VERSION_STRING_COMPLETE ". built "__DATE__" "__TIME__" using " LUA_VERSION "\n");
	printf("by Magnus Auvinen (magnus.auvinen@gmail.com)\n");
	printf("\n");
	printf("%s [OPTIONS] [TARGETS]\n", program_name);
	printf("\n");

	for(j = 0; options[j].sw; j++)
		printf("  %-20s %s\n", options[j].sw, options[j].desc);

	printf("\n");
}

/* signal handler */
static void abortsignal(int i)
{
	(void)i;
	printf("signal cought! exiting.\n");
	exit(1);
}

static int parse_parameters(int num, char **params)
{
	int i, j;
	
	/* parse parameters */
	for(i = 0; i < num; ++i)
	{
		for(j = 0; options[j].sw; j++)
		{
			if(strcmp(params[i], options[j].sw) == 0)
			{
				if(options[j].s)
				{
					if(i+1 >= num)
					{
						printf("%s: you must supply a argument with %s\n", program_name, options[j].sw);
						return 1;
					}
					
					i++;
					*options[j].s = params[i];
				}
				else
					*options[j].v = 1;

				j = -1;
				break;
			}
		}
		
		if(j != -1)
		{
			/* check if it's an option, and warn if it could not be found */
			if(params[i][0] == '-')
			{
				printf("%s: unknown switch '%s'\n", program_name, params[i]);
				return 1;
			}
			else
			{
				/* check for = because it indicates if it's a target or script argument */
				if(strchr(params[i], '='))
				{
					option_scriptargs[option_num_scriptargs] = params[i];
					option_num_scriptargs++;
				}
				else
				{
					option_targets[option_num_targets] = params[i];
					option_num_targets++;
				}
			}
		}
	}
	return 0;
}

/* ********* */
int main(int argc, char **argv)
{
	int i, error;

	/* init platform */
	install_signals(abortsignal);
	platform_init();

	/* fetch program name, if we can */
	for(i = 0; argv[0][i]; ++i)
	{
		if(argv[0][i] == '/' || argv[0][i] == '\\')
			program_name = &argv[0][i+1];
	}

	/* get basescript */
	if(getenv("BAM_BASESCRIPT"))
		option_basescript = getenv("BAM_BASESCRIPT");

	if(getenv("BAM_OPTIONS"))
	{
		/* TODO: implement this */
	}
	
	parse_parameters(argc-1, argv+1);
		
	/* parse the report str */
	for(i = 0; option_report_str[i]; i++)
	{
		if(option_report_str[i] == 'c')
			option_report_color = 1;
		else if(option_report_str[i] == 'b')
			option_report_bar = 1;
		else if(option_report_str[i] == 's')
			option_report_steps = 1;
	}
	
	/* convert the threads string */
	option_threads = atoi(option_threads_str);
	if(option_threads < 0)
	{
		printf("%s: invalid number of threads supplied\n", program_name);
		return 1;
	}
	
	/* check for help argument */
	if(option_print_help)
	{
		print_help();
		return 0;
	}

	/* */
	if(option_debug_dumpinternal)
	{
		int f;
		for(f = 0; internal_files[f].filename; f++)
		{
			printf("%s:\n", internal_files[f].filename);
			puts(internal_files[f].content);
		}
				
		return 0;
	}
		
	/* check if a script exist */
	if(!file_exist(option_script))
	{
		printf("%s: no project named '%s'\n", program_name, option_script);
		return 1;
	}
	
	/* init the context */
	/* TODO: search for a bam file */
	error = bam(option_script, option_targets, option_num_targets);
	
	platform_shutdown();

	/* error could be some high value like 256 */
	/* seams like this could be clamped down to a */
	/* unsigned char and not be an error anymore */
	if(error)
		return 1;
	return 0;
}
