/* some parts written at high altitudes */
/* some parts written at the top of the eiffel tower */

/* system includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* lua includes */
#include <lua.h>
#include <lualib.h> /* luaL_openlibs */
#include <lauxlib.h> /* luaL_loadfile */

/* program includes */
#include "mem.h"
#include "node.h"
#include "path.h"
#include "support.h"
#include "dep_cpp.h"
#include "context.h"
#include "cache.h"
#include "luafuncs.h"
#include "platform.h"
#include "session.h"
#include "version.h"

/* internal base.bam file */
#include "internal_base.h"

/* needed for getcwd */
#if defined(BAM_FAMILY_UNIX) || defined(BAM_FAMILY_BEOS)
	#include <unistd.h>
#endif

#ifdef BAM_FAMILY_WINDOWS
	#include <direct.h>
	#define getcwd _getcwd /* stupid msvc is calling getcwd non-ISO-C++ conformant */
	#define CACHE_FILENAME "bamcache.dat"
#else
	#define CACHE_FILENAME ".bamcache"
#endif

#define DEFAULT_REPORT_STYLE "s"

/* ** */
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
static int option_no_cache = 0;
static int option_dry = 0;
static int option_dependent = 0;
static int option_abort_on_error = 0;
static int option_gc = 1;
static int option_debug_nodes = 0;
static int option_debug_jobs = 0;
static int option_debug_dumpinternal = 0;
static int option_debug_nointernal = 0;

static int option_print_help = 0;

static const char *option_cache = CACHE_FILENAME; /* -k filename*/
static const char *option_script = "bam.lua"; /* -f filename */
static const char *option_threads_str = "0";
static const char *option_report_str = DEFAULT_REPORT_STYLE;
static const char *option_targets[128] = {0};
static int option_num_targets = 0;
static const char *option_scriptargs[128] = {0};
static int option_num_scriptargs = 0;

/* session object */
struct SESSION session = {
	"bam",  /* exe */
	"bam", /* name */
	1, /* threads */
	0 /* rest */
};

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
		TODO: Add documentation
	@END*/

	/*@OPTION Script File ( -s FILENAME )
		Bam file to use. In normal operation, Bam executes
		^bam.lua^. This option allows you to specify another bam
		file.
	@END*/
	{&option_script,0			, "-s", "bam file to use. default is bam.lua"},

	/*@OPTION Clean ( -c )
		Cleans the specified targets or the default target.
	@END*/
	{0, &option_clean			, "-c", "clean"},

	/*@OPTION Dependent build ( -d )
		Builds all targets that are dependent on the given targets.
		If no targets are given this option doesn't do anything.
	@END*/
	{0, &option_dependent			, "-d", "build targets that is dependent given targets"},
	
	/*@OPTION Cache ( -k FILENAME )
		Specifies what cache file to use instead of the default.
	@END*/
	{&option_cache,0					, "-k", "use cache (" CACHE_FILENAME ")"},

	/*@OPTION No cache ( -n )
		Do not use cache when building.
	@END*/
	{0, &option_no_cache			, "-n", "don't use cache (" CACHE_FILENAME ") (no enabled)"},
	
	/*@OPTION Verbose ( -v )
		Prints all commands that are runned when building.
	@END*/
	{0, &session.verbose			, "-v", "verbose"},

	/*@OPTION Script Backtrace ( -bt )
		Prints backtrace when there is a script error
	@END*/
	{0, &session.lua_backtrace		, "-bt", "lua backtrace"},

	/*@OPTION Script Locals ( -l )
		Prints local and up values in the backtrace when there is a script error
	@END*/
	{0, &session.lua_locals		, "-l", "lua locals"},

	/*@OPTION Threading ( -j N )
		Sets the number of threads used when building. A good value for N is
		the same number as logical cores on the machine. Set to 0 to disable.
	@END*/
	{&option_threads_str,0		, "-j", "sets the number of threads to use. 0 to disables"},

	/*@OPTION Garbage collector ( -gc )
		Enables the lua garbage collector. Reduces memory footprint when
		executing the build scripts by sacrificing performace. Only needed
		on memory restricted systems.
	@END*/
	/*{0, &option_gc			, "-gc", "garbage collector"},*/

	/*@OPTION Abort on error ( -a )
		Setting this will cause bam to abort the build process when an error has occured.
		Normally it would continue as far as it can
	@END*/
	{0,&option_abort_on_error	, "-a", "abort on error"},

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
	{&option_report_str,0		, "-r", "set report format (default:" DEFAULT_REPORT_STYLE ")\n"
		"                       " "    b = bar, s = steps, c = color"},

	/*@OPTION Dry Run ( --dry )
		Does everything that it normally would do but does not execute any
		commands.
	@END*/
	{0, &option_dry				, "--dry", "dry run"},
	
	/*@OPTION Debug: Dump Nodes ( --debug-nodes )
		Dumps all nodes in the dependency graph, their state and their
		dependent nodes. This is useful if you are writing your own
		actions to verify that dependencies are correctly added.
	@END*/
	{0, &option_debug_nodes		, "--debug-nodes", "prints all the nodes with dependencies"},

	/*@OPTION Debug: Dump Jobs ( --debug-jobs )
	@END*/
	{0, &option_debug_jobs		, "--debug-jobs", "prints all the jobs that exist"},

	/*@OPTION Debug: Dump Internal Scripts ( --debug-dump-int )
	@END*/
	{0, &option_debug_dumpinternal		, "--debug-dump-int", "dumps the internals scripts to stdout"},

	/*@OPTION Debug: No Internal ( --debug-no-int )
		Disables all the internal scripts that bam loads on startup.
	@END*/
	{0, &option_debug_nointernal		, "--debug-no-int", "don't load internal scripts"},
		
	/* terminate list */
	{0, 0, (const char*)0, (const char*)0}
};

static const char *internal_base_reader(lua_State *L, void *data, size_t *size)
{
	char **p = (char **)data;
	if(!*p)
		return 0;
		
	*size = strlen(*p);
	data = *p;
	*p = 0;
	return data;
}


static void lua_setglobalstring(lua_State *L, const char *field, const char *s)
{
	lua_pushstring(L, s);
	lua_setglobal(L, field);
}

/* *** */
int register_lua_globals(struct CONTEXT *context)
{
	int i, error = 0;
		
	/* add standard libs */
	luaL_openlibs(context->lua);
	
	/* add specific functions */
	lua_register(context->lua, L_FUNCTION_PREFIX"add_job", lf_add_job);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_output", lf_add_output);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_pseudo", lf_add_pseudo);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_dependency", lf_add_dependency);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_dependency_search", lf_add_dependency_search);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_constraint_shared", lf_add_constraint_shared);
	lua_register(context->lua, L_FUNCTION_PREFIX"add_constraint_exclusive", lf_add_constraint_exclusive);
	lua_register(context->lua, L_FUNCTION_PREFIX"default_target", lf_default_target);
	lua_register(context->lua, L_FUNCTION_PREFIX"set_touch", lf_set_touch);
	lua_register(context->lua, L_FUNCTION_PREFIX"set_filter", lf_set_filter);

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
	lua_register(context->lua, L_FUNCTION_PREFIX"loadfile", lf_loadfile);
	
	lua_register(context->lua, L_FUNCTION_PREFIX"mkdir", lf_mkdir);
	lua_register(context->lua, L_FUNCTION_PREFIX"fileexist", lf_fileexist);

	lua_register(context->lua, L_FUNCTION_PREFIX"isstring", lf_isstring);
	lua_register(context->lua, L_FUNCTION_PREFIX"istable", lf_istable);

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
		char cwd[MAX_PATH_LENGTH];
		if(!getcwd(cwd, sizeof(cwd)))
		{
			printf("%s: error: couldn't get current working directory\n", session.name);
			return -1;
		}
		
		lua_setglobalstring(context->lua, CONTEXT_LUA_PATH, context->script_directory);
		lua_setglobalstring(context->lua, CONTEXT_LUA_WORKPATH, cwd);
	}

	/* set version, family, platform, arch, verbocity */
	lua_setglobalstring(context->lua, "_bam_version", BAM_VERSION_STRING);
	lua_setglobalstring(context->lua, "_bam_version_complete", BAM_VERSION_STRING_COMPLETE);
	lua_setglobalstring(context->lua, "family", BAM_FAMILY_STRING);
	lua_setglobalstring(context->lua, "platform", BAM_PLATFORM_STRING);
	lua_setglobalstring(context->lua, "arch", BAM_ARCH_STRING);
	lua_setglobalstring(context->lua, "_bam_exe", session.exe);
	lua_pushnumber(context->lua, session.verbose);
	lua_setglobal(context->lua, "verbose");

	/* load base script */
	if(!option_debug_nointernal)
	{
		int ret;
		const char *p;
		int f;
		
		for(f = 0; internal_files[f].filename; f++)
		{
			p = internal_files[f].content;
			
			if(session.verbose)
				printf("%s: reading internal file '%s'\n", session.name, internal_files[f].filename);
		
			lua_getglobal(context->lua, "errorfunc");
			
			/* push error function to stack */
			ret = lua_load(context->lua, internal_base_reader, (void *)&p, internal_files[f].filename);
			if(ret != 0)
			{
				lf_errorfunc(context->lua);
				
				if(ret == LUA_ERRSYNTAX)
				{
				}
				else if(ret == LUA_ERRMEM)
					printf("%s: memory allocation error\n", session.name);
				else
					printf("%s: unknown error parsing base script\n", session.name);
					
				error = 1;
			}
			else if(lua_pcall(context->lua, 0, LUA_MULTRET, -2) != 0)
				error = 1;
		}
	}
	
	return error;
}

static int allocs = 0;

static void *lua_alloctor_malloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	allocs++;
	if (nsize == 0)
	{
		free(ptr);
		return NULL;
	}
	
	return realloc(ptr, nsize);
}

static struct HEAP *luaheap;

static void *lua_alloctor_incheap(void *ud, void *ptr, size_t osize, size_t nsize)
{
	void *new_ptr;
	(void)ud; /* not used */

	allocs++;

	if(nsize == 0)
		return NULL;

	new_ptr = mem_allocate(luaheap, nsize);
	if(osize)
	{
		if(osize < nsize)
			osize = nsize;
		memcpy(new_ptr, ptr, osize);
	}

	return new_ptr;
}

/* returns 0 on no find, 1 on find and -1 on error */
static int lookup_checkpath(struct CONTEXT *context, struct NODE *node, const char *path)
{
	struct NODE *depnode;

	/* search up the node and add it if we need */
	depnode = node_find(context->graph, path);
	if(depnode)
	{
		if(!node_add_dependency_withnode(node, depnode))
			return -1;
		return 1;
	}
	
	/* check if it exists on the disk */
	if(file_exist(path))
	{
		if(!node_add_dependency(node, path))
			return -1;
		return 1;
	}
	
	return 0;
}

/* this functions takes the whole deferred lookup list and searches for the file */
static int lookup_deferred_searches(struct CONTEXT *context)
{
	struct LOOKUP *lookup;
	struct STRINGLIST *dep;
	struct STRINGLIST *path;
	char buffer[MAX_PATH_LENGTH];
	size_t total;
	int result;
	
	for(lookup = context->firstlookup; lookup; lookup = lookup->next)
	{
		for(dep = lookup->firstdep; dep; dep = dep->next)
		{
			/* check the current directory */
			result = lookup_checkpath(context, lookup->node, dep->str);
			if(result == 1)
				continue;
			if(result == -1)
				return -1;

			/* check all the other directories */
			for(path = lookup->firstpath; path; path = path->next)
			{
				/* +1 for the extra / */
				total = dep->len+path->len+1;
				
				/* +1 for null */
				if(total+1 >= MAX_PATH_LENGTH)
				{
					printf("%s: error: '%s/%s' is longer then %d", session.name, path->str, dep->str, MAX_PATH_LENGTH);
					return -1;
				}

				/* build the path, "%s/%s" */
				memcpy(buffer, path->str, path->len);
				buffer[path->len] = '/';
				memcpy(buffer+path->len+1, dep->str, dep->len);
				buffer[total] = 0;
				
				result = lookup_checkpath(context, lookup->node, buffer);
				if(result == 1)
					break;
				if(result == -1)
					return -1;
			}
		}
	}
	
	return 0;
}

static int bam_setup(struct CONTEXT *context, const char *scriptfile, const char **targets, int num_targets)
{
	/* */	
	if(session.verbose)
		printf("%s: setup started\n", session.name);
	
	/* set filename */
	context->filename = scriptfile;
	context->filename_short = path_filename(scriptfile);
	
	/* set global timestamp to the script file */
	context->globaltimestamp = file_timestamp(scriptfile);
	
	/* fetch script directory */
	{
		char cwd[MAX_PATH_LENGTH];
		char path[MAX_PATH_LENGTH];

		if(!getcwd(cwd, sizeof(cwd)))
		{
			printf("%s: error: couldn't get current working directory\n", session.name);
			return -1;
		}
		
		if(path_directory(context->filename, path, sizeof(path)))
		{
			printf("%s: error: path too long '%s'\n", session.name, path);
			return -1;
		}
		
		if(path_join(cwd, path, context->script_directory, sizeof(context->script_directory)))
		{
			printf("%s: error: path too long when joining '%s' and '%s'\n", session.name, cwd, path);
			return -1;
		}
	}
	
	/* register all functions */
	if(register_lua_globals(context) != 0)
	{
		printf("%s: error: registering of lua functions failed\n", session.name);
		return -1;
	}

	/* load script */	
	if(session.verbose)
		printf("%s: reading script from '%s'\n", session.name, scriptfile);
		
	/* push error function to stack and load the script */
	lua_getglobal(context->lua, "errorfunc");
	switch(luaL_loadfile(context->lua, scriptfile))
	{
		case 0: break;
		case LUA_ERRSYNTAX:
			lf_errorfunc(context->lua);
			return -1;
		case LUA_ERRMEM: 
			printf("%s: memory allocation error\n", session.name);
			return -1;
		case LUA_ERRFILE:
			printf("%s: error opening '%s'\n", session.name, scriptfile);
			return -1;
		default:
			printf("%s: unknown error\n", session.name);
			return -1;
	}

	/* call the code chunk */	
	if(lua_pcall(context->lua, 0, LUA_MULTRET, -2) != 0)
	{
		printf("%s: script error (-bt for more detail)\n", session.name);
		return -1;
	}
	
	/* process deferred lookups */
	if(lookup_deferred_searches(context) != 0)
		return -1;
	
	/* */	
	if(session.verbose)
		printf("%s: making build target\n", session.name);
	
	/* make build target */
	{
		struct NODE *node;
		int all_target = 0;
		int i;

		if(node_create(&context->target, context->graph, "_bam_buildtarget", 0, 0))
			return -1;
			
		if(num_targets)
		{
			/* search for all target */
			for(i = 0; i < num_targets; i++)
			{
				if(strcmp(targets[i], "all") == 0)
				{
					all_target = 1;
					break;
				}
			}
		}
		
		/* default too all if we have no targets or default target */
		if(num_targets == 0 && !context->defaulttarget)
			all_target = 1;
		
		if(all_target)
		{
			/* build the all target */
			for(node = context->graph->first; node; node = node->next)
			{
				if(node->firstparent == NULL && node != context->target)
				{
					if(!node_add_dependency_withnode(context->target, node))
						return -1;
				}
			}
		}
		else
		{
			if(num_targets)
			{
				for(i = 0; i < num_targets; i++)
				{
					struct NODE *node = node_find(context->graph, targets[i]);
					if(!node)
					{
						printf("%s: target '%s' not found\n", session.name, targets[i]);
						return -1;
					}
					
					if(option_dependent)
					{
						/* TODO: this should perhaps do a reverse walk up in the tree to
							find all dependent node with commandline */
						struct NODELINK *parent;
						for(parent = node->firstparent; parent; parent = parent->next)
						{
							if(!node_add_dependency_withnode(context->target, parent->node))
								return -1;
						}
								
					}
					else
					{
						if(!node_add_dependency_withnode(context->target, node))
							return -1;
					}
				}
			}
			else
			{
				if(!node_add_dependency_withnode(context->target, context->defaulttarget))
					return -1;
			}

		}
	}
	
	/* */	
	if(session.verbose)
		printf("%s: setup done\n", session.name);
	
	
	/* return success */
	return 0;
}

/* *** */
static int bam(const char *scriptfile, const char **targets, int num_targets)
{
	struct CONTEXT context;
	int build_error = 0;
	int setup_error = 0;
	int report_done = 0;

	/* build time */
	time_t starttime  = time(0x0);
	
	/* zero out and create memory heap, graph */
	memset(&context, 0, sizeof(struct CONTEXT));
	context.graphheap = mem_create();
	context.lookupheap = mem_create();
	context.graph = node_create_graph(context.graphheap);
	context.exit_on_error = option_abort_on_error;
	context.buildtime = timestamp();

	/* create lua context */
	/* HACK: Store the context pointer as the userdata pointer to the allocator to make
		sure that we have fast access to it. This makes the context_get_pointer call very fast */
	if(option_gc)
		context.lua = lua_newstate(lua_alloctor_malloc, &context);
	else
	{
		context.luaheap = mem_create();
		luaheap = context.luaheap; /*mem_create();*/
		context.lua = lua_newstate(lua_alloctor_incheap, &context);

		/* we don't need the gc, just run and destroy it later */
		lua_gc(context.lua, LUA_GCSTOP, 0);
	}

	/* install panic function */
	lua_atpanic(context.lua, lf_panicfunc);

	/* load cache (thread?) */
	if(option_no_cache == 0)
		context.cache = cache_load(option_cache);

	/* do the setup */
	setup_error = bam_setup(&context, scriptfile, targets, num_targets);

	/* done with the loopup heap */
	mem_destroy(context.lookupheap);

	/* if we have a separate lua heap, just destroy it */
	if(context.luaheap)
		mem_destroy(context.luaheap);
	else
		lua_close(context.lua);
	
	/* do actions if we don't have any errors */
	if(!setup_error)
	{
		build_error = context_build_prepare(&context);
		
		if(!build_error)
		{
			if(option_debug_nodes)
			{
				/* debug dump all nodes */
				node_debug_dump(context.graph);
			}
			else if(option_debug_jobs)
			{
				/* debug dump all jobs */
				node_debug_dump_jobs(context.graph);
			}
			else if(option_dry)
			{
			}
			else
			{
				/* run build or clean */
				if(option_clean)
					build_error = context_build_clean(&context);
				else
				{
					build_error = context_build_make(&context);
					report_done = 1;
				}
			}
		}
	}		

	/* save cache (thread?) */
	if(option_no_cache == 0 && setup_error == 0)
		cache_save(option_cache, context.graph);
	
	/* clean up */
	mem_destroy(context.graphheap);

	/* print final report and return */
	if(setup_error)
	{
		/* no error message on setup error, it reports fine itself */
		return setup_error;
	}
	else if(build_error)
		printf("%s: error during build\n", session.name);
	else if(report_done)
	{
		if(context.num_commands == 0)
			printf("%s: targets are up to date already\n", session.name);
		else
		{
			time_t s = time(0x0) - starttime;
			printf("%s: done (%d:%.2d)\n", session.name, (int)(s/60), (int)(s%60));
		}
	}

	return build_error;
}



/* signal handler */
static void abortsignal(int i)
{
	(void)i;
	printf("%s: signal cought, waiting for jobs to finish\n", session.name);
	session.abort = 1;
}


void install_abort_signal()
{
	install_signals(abortsignal);
}

/* */
static void print_help()
{
	int j;
	printf("bam version " BAM_VERSION_STRING_COMPLETE ". built "__DATE__" "__TIME__" using " LUA_VERSION "\n");
	printf("by Magnus Auvinen (magnus.auvinen@gmail.com)\n");
	printf("\n");
	printf("%s [OPTIONS] [TARGETS]\n", session.name);
	printf("\n");

	for(j = 0; options[j].sw; j++)
		printf("  %-20s %s\n", options[j].sw, options[j].desc);

	printf("\n");
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
						printf("%s: you must supply a argument with %s\n", session.name, options[j].sw);
						return -1;
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
				printf("%s: unknown switch '%s'\n", session.name, params[i]);
				return -1;
			}
			else
			{
				/* check for = because it indicates if it's a target or script argument */
				if(strchr(params[i], '='))
					option_scriptargs[option_num_scriptargs++] = params[i];
				else
					option_targets[option_num_targets++] = params[i];
			}
		}
	}
	return 0;
}

static int parse_parameters_str(const char *str)
{
	char *ptrs[64];
	int num_ptrs = 0;
	char buffer[1024];
	char *start = buffer;
	char *current = start;
	char *end = buffer+sizeof(buffer);
	int string_parse = 0;
	
	ptrs[0] = start;
	
	while(1)
	{
		/* fetch next character */
		char c = *str++;
		
		if(string_parse)
		{
			if(c == '"')
				string_parse = 0;
			else if(*str == 0)
			{
				printf("%s: error: unterminated \"\n", session.name);
				return -1;	
			}
			else
				*current++ = c;
		}
		else
		{			
			if(c == ' ' || c == '\t' || c == 0)
			{
				/* zero term and add this ptr */
				*current++ = 0;
				num_ptrs++;
				ptrs[num_ptrs] = current;
			}
			else if(c == '"')
				string_parse = 1;
			else
				*current++ = c;
		}
		
		if(current == end)
		{
			printf("%s: error: argument too long\n", session.name);
			return -1;
		}
		
		/* break out on zero term */
		if(!c)
			break;
	}

	/* parse all the parameters */	
	return parse_parameters(num_ptrs, ptrs);
}

/* ********* */
int main(int argc, char **argv)
{
	int i, error;

	/* init platform */
	platform_init();

	/* set exe */
	session.exe = argv[0];

	/* fetch program name, if we can */
	for(i = 0; argv[0][i]; ++i)
	{
		if(argv[0][i] == '/' || argv[0][i] == '\\')
			session.name = &argv[0][i+1];
	}

	/* parse environment parameters */
	if(getenv("BAM_OPTIONS"))
	{
		if(parse_parameters_str(getenv("BAM_OPTIONS")))
			return -1;
	}
	
	/* parse commandline parameters */
	if(parse_parameters(argc-1, argv+1))
		return -1;
		
	/* parse the report str */
	for(i = 0; option_report_str[i]; i++)
	{
		if(option_report_str[i] == 'c')
			session.report_color = 1;
		else if(option_report_str[i] == 'b')
			session.report_bar = 1;
		else if(option_report_str[i] == 's')
			session.report_steps = 1;
	}
	
	/* convert the threads string */
	session.threads = atoi(option_threads_str);
	if(session.threads < 0)
	{
		printf("%s: invalid number of threads supplied\n", session.name);
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
	
	/* init the context */
	error = bam(option_script, option_targets, option_num_targets);
	
	platform_shutdown();

	/* error could be some high value like 256 seams like this could */
	/* be clamped down to a unsigned char and not be an error anymore */
	if(error)
		return 1;
	return 0;
}
