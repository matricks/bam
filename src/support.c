#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "platform.h"
#include "path.h"
#include "context.h"
#include "session.h"
#include "support.h"

#ifdef BAM_FAMILY_BEOS
    #include <sched.h>
#endif

#ifdef BAM_FAMILY_WINDOWS
	/* windows code */
	#define WIN32_LEAN_AND_MEAN
	#define VC_EXTRALEAN
	#include <windows.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/utime.h>
	#include <signal.h>
	#include <direct.h> /* _mkdir */
	
	static void list_directory(const char *path, void (*callback)(const char *filename, int dir, void *user), void *user)
	{
		WIN32_FIND_DATA finddata;
		HANDLE handle;
		char buffer[1024*2];
		char *startpoint;

		if(path[0])
		{
			strcpy(buffer, path);
			strcat(buffer, "/*");
			startpoint = buffer + strlen(path)+1;
		}
		else
		{
			strcpy(buffer, "*");
			startpoint = buffer;
		}

		handle = FindFirstFileA(buffer, &finddata);

		if (handle == INVALID_HANDLE_VALUE)
			return;

		/* add all the entries */
		do
		{
			strcpy(startpoint, finddata.cFileName);
			if(finddata.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)
				callback(buffer, 1, user);
			else
				callback(buffer, 0, user);
		} while (FindNextFileA(handle, &finddata));

		FindClose(handle);
	}

	/* signals. should be moved to platform.c or similar? */
	void install_signals(void (*abortsignal)(int))
	{
		signal(SIGINT, abortsignal);
		signal(SIGBREAK, abortsignal);
	}

	static CRITICAL_SECTION criticalsection;
	void platform_init()
	{
		InitializeCriticalSection(&criticalsection);
		
		/* this environment variable is set by Microsoft Visual Studio
			when building. It causes cl.exe to redirect it's output to
			the specified pipe id. this causes loads of problems with
			output.
		*/ 
		SetEnvironmentVariable("VS_UNICODE_OUTPUT", NULL);
	}
	
	void platform_shutdown() { DeleteCriticalSection(&criticalsection); }
	void criticalsection_enter() { EnterCriticalSection(&criticalsection); }
	void criticalsection_leave() { LeaveCriticalSection(&criticalsection); }

	void *threads_create(void (*threadfunc)(void *), void *u)
	{
		return CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)threadfunc, u, 0, NULL);
	}

	void threads_join(void *thread)
	{
		WaitForSingleObject((HANDLE)thread, INFINITE);
	}

	void threads_yield()
	{
		Sleep(1);
	}
	
	PLUGINFUNC plugin_load(const char *filename)
	{
		char buffer[MAX_PATH_LENGTH];
		HMODULE handle;
		FARPROC func;
		
		_snprintf(buffer, sizeof(buffer), "%s.dll", filename);

		handle = LoadLibrary(buffer);
		if(handle == NULL)
		{
			fprintf(stderr, "error loading plugin '%s'\n", buffer);
			return NULL;
		}
		
		func = GetProcAddress(handle, "plugin_main");
		if(func == NULL)
		{
			CloseHandle(handle);
			fprintf(stderr, "error fetching plugin main from '%s'\n", buffer);
			return NULL;
		}
		
		return (PLUGINFUNC)func;
	}	

#else
	#define D_TYPE_HACK
	/* TODO: detect DT_DIR/DT_UNKNOWN */

#ifdef D_TYPE_HACK
	#define __USE_BSD
#endif
	#include <dirent.h>
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/signal.h>
	#include <sys/stat.h>
	#include <utime.h>
	#include <pthread.h>

	static void list_directory(const char *path, void (*callback)(const char *filename, int dir, void *user), void *user)
	{
		DIR *dir;
		struct dirent *entry;
#ifndef D_TYPE_HACK
		struct stat info;
#endif
		char buffer[1024];
		char *startpoint;
		
		if(*path == 0) /* special case for current directory */
		{
			dir = opendir(".");
			startpoint = buffer;
		}
		else
		{
			dir = opendir(path);

			/* get starting point and append a slash */
			strcpy(buffer, path);
			startpoint = buffer + strlen(buffer);
			*startpoint = '/';
			startpoint++;		
		}
		
		if(!dir)
			return;
		
		while((entry = readdir(dir)) != NULL)
		{
			/* make the path absolute */
			strcpy(startpoint, entry->d_name);

#ifdef D_TYPE_HACK			
			/* TODO: support DT_UNKNOWN */
			/* call the callback */
			if(entry->d_type == DT_DIR)
				callback(buffer, 1, user);
			else
				callback(buffer, 0, user);
#else
			/* do stat to obtain if it's a directory or not */
			stat(buffer, &info);
			if(S_ISDIR(info.st_mode))
				callback(buffer, 1, user);
			else
				callback(buffer, 0, user);
#endif
		}
		
		closedir(dir);
	}

	/* signals. should be moved to platform.c or similar? */
	void install_signals(void (*abortsignal)(int))
	{
		signal(SIGHUP, abortsignal);
		signal(SIGINT, abortsignal);
		signal(SIGKILL, abortsignal);
	}

	static pthread_mutex_t lock_mutex = PTHREAD_MUTEX_INITIALIZER;

	void platform_init() {}
	void platform_shutdown() {}
	void criticalsection_enter() { pthread_mutex_lock(&lock_mutex); }
	void criticalsection_leave() { pthread_mutex_unlock(&lock_mutex); }

	void *threads_create(void (*threadfunc)(void *), void *u)
	{
		pthread_t id;
		pthread_create(&id, NULL, (void *(*)(void*))threadfunc, u);
		return (void*)id;
	}

	void threads_join(void *thread)
	{
		pthread_join((pthread_t)thread, NULL);
	}

	void threads_yield()
	{
		sched_yield();
	}
	
	PLUGINFUNC plugin_load(const char *filename)
	{
		char buffer[MAX_PATH_LENGTH];
		const char *error;
		void *handle;
		void *func;
		
		snprintf(buffer, sizeof(buffer), "./%s.so", filename);

		handle = dlopen(buffer, RTLD_LAZY);
		if(!handle)
		{
			fputs(dlerror(), stderr);
			fputs("\n", stderr);
			
			return NULL;
		}
		
		func = dlsym(handle, "plugin_main");
		error = dlerror();

		if(error)
		{
			fputs(error, stderr);
			fputs("\n", stderr);
			dlclose(handle);
			return NULL;
		}
		
		return (PLUGINFUNC)func;
	}
#endif

time_t timestamp() { return time(NULL); }

time_t file_timestamp(const char *filename)
{
#ifdef BAM_PLATFORM_MACOSX
	/* Mac OS X version */
		struct stat s;
		if(stat(filename, &s) == 0)
			return s.st_mtimespec.tv_sec;
		return 0;

#else		
	/* *NIX version and windows version*/
	struct stat s;
	if(stat(filename, &s) == 0)
		return s.st_mtime;
	return 0;
#endif
}

int file_createdir(const char *path)
{
	int r;
#ifdef BAM_FAMILY_WINDOWS
	r = _mkdir(path);
#else
	r = mkdir(path, 0755);
#endif
	if(r == 0 || errno == EEXIST)
		return 0;
	return -1;
}

void file_touch(const char *filename)
{
#ifdef BAM_FAMILY_WINDOWS
	_utime(filename, NULL);
#else
	utime(filename, NULL);
#endif
}

#ifdef BAM_FAMILY_WINDOWS
static void passthru(FILE *fp)
{
	while(1)
	{
		char buffer[1024*4];
		size_t num_bytes = fread(buffer, 1, sizeof(buffer), fp);
		if(num_bytes <= 0)
			break;
		criticalsection_enter();
		fwrite(buffer, 1, num_bytes, stdout);
		criticalsection_leave();
	}
}
#endif

int run_command(const char *cmd, const char *filter)
{
	int ret;
#ifdef BAM_FAMILY_WINDOWS
	FILE *fp = _popen(cmd, "r");

	if(!fp)
		return -1;
		
	if(filter && *filter == 'F')
	{
		/* first filter match */
		char buffer[1024];
		size_t total;
		size_t matchlen;
		size_t numread;
		
		/* skip first letter */
		filter++;
		matchlen = strlen(filter);
		total = 0;

		while(1)
		{
			numread = fread(buffer+total, 1, matchlen-total, fp);
			if(numread <= 0)
			{
				/* done or error, flush and exit */
				fwrite(buffer, 1, total, stdout);
				break;
			}
			
			/* accumelate the bytes read */
			total += numread;
			
			if(total >= matchlen)
			{
				/* check if it matched */
				if(memcmp(buffer, filter, matchlen) == 0)
				{
					/* check for line ending */
					char t = fgetc(fp);
					if(t == '\r')
					{
						/* this can be CR or CR/LF */
						t = fgetc(fp);
						if(t != '\n')
						{
							/* not a CR/LF */
							fputc(t, stdout);
						}
					}
					else if(t == '\n')
					{
						/* normal LF line ending */
					}
					else
					{
						/* no line ending */
						fputc(t, stdout);
					}
				}
				else
				{
					fwrite(buffer, 1, total, stdout);
				}
	
				passthru(fp);
				break;
			}
			
		}
	}
	else
	{
		/* no filter */
		passthru(fp);
	}

	ret = _pclose(fp);
#else
	ret = system(cmd);
#endif
	if(session.verbose)
		printf("%s: ret=%d %s\n", session.name, ret, cmd);
	return ret;
}

/* general */
int file_exist(const char *filename)
{
	struct stat s;
	if(stat(filename, &s) == 0)
		return 1;
	return 0;
}

/* list directory functionallity */
typedef struct
{
	lua_State *lua;
	int i;
} LISTDIR_CALLBACK_INFO;

static void listdir_callback(const char *filename, int dir, void *user)
{
	LISTDIR_CALLBACK_INFO *info = (LISTDIR_CALLBACK_INFO *)user;
	lua_pushstring(info->lua, filename);
	lua_rawseti(info->lua, -2, info->i++);
}

int lf_listdir(lua_State *L)
{
	LISTDIR_CALLBACK_INFO info;
	info.lua = L;
	info.i = 1;
	
	/* create the table */
	lua_newtable(L);

	/* add all the entries */
	if(strlen(lua_tostring(L, 1)) < 1)
		list_directory(context_get_path(L), listdir_callback, &info);
	else
	{
		char buffer[1024];
		path_join(context_get_path(L), -1, lua_tostring(L,1), -1, buffer, sizeof(buffer));
		list_directory(buffer, listdir_callback, &info);
	}

	return 1;
}

/* collect functionallity */
enum
{
	COLLECTFLAG_FILES=1,
	COLLECTFLAG_DIRS=2,
	COLLECTFLAG_HIDDEN=4,
	COLLECTFLAG_RECURSIVE=8
};

typedef struct
{
	int path_len;
	const char *start_str;
	int start_len;
	
	const char *end_str;
	int end_len;
	
	lua_State *lua;
	int i;
	int flags;
} COLLECT_CALLBACK_INFO;

static void run_collect(COLLECT_CALLBACK_INFO *info, const char *input);

static void collect_callback(const char *filename, int dir, void *user)
{
	COLLECT_CALLBACK_INFO *info = (COLLECT_CALLBACK_INFO *)user;
	const char *no_pathed = filename + info->path_len;
	int no_pathed_len = strlen(no_pathed);

	/* don't process . and .. paths */
	if(filename[0] == '.')
	{
		if(filename[1] == 0)
			return;
		if(filename[1] == '.' && filename[2] == 0)
			return;
	}
	
	/* don't process hidden stuff if not wanted */
	if(no_pathed[0] == '.' && !(info->flags&COLLECTFLAG_HIDDEN))
		return;

	do
	{
		/* check end */
		if(info->end_len > no_pathed_len || strcmp(no_pathed+no_pathed_len-info->end_len, info->end_str))
			break;

		/* check start */
		if(info->start_len && strncmp(no_pathed, info->start_str, info->start_len))
			break;
		
		/* check dir vs search param */
		if(!dir && info->flags&COLLECTFLAG_DIRS)
			break;
		
		if(dir && info->flags&COLLECTFLAG_FILES)
			break;
			
		/* all criterias met, push the result */
		lua_pushstring(info->lua, filename);
		lua_rawseti(info->lua, -2, info->i++);
	} while(0);
	
	/* recurse */
	if(dir && info->flags&COLLECTFLAG_RECURSIVE)
	{
		char recursepath[1024];
		COLLECT_CALLBACK_INFO recurseinfo = *info;
		strcpy(recursepath, filename);
		strcat(recursepath, "/");
		strcat(recursepath, info->start_str);
		run_collect(&recurseinfo, recursepath);
		info->i = recurseinfo.i;
	}	
}

static void run_collect(COLLECT_CALLBACK_INFO *info, const char *input)
{
	char dir[1024];
	int dirlen = 0;
	
	/* get the directory */
	path_directory(input, dir, sizeof(dir));
	dirlen = strlen(dir);
	info->path_len = dirlen+1;
	
	/* set the start string */
	if(dirlen)
		info->start_str = input + dirlen + 1;
	else
		info->start_str = input;
		
	for(info->start_len = 0; info->start_str[info->start_len]; info->start_len++)
	{
		if(info->start_str[info->start_len] == '*')
			break;
	}
	
	/* set the end string */
	if(info->start_str[info->start_len])
		info->end_str = info->start_str + info->start_len + 1;
	else
		info->end_str = info->start_str + info->start_len;
	info->end_len = strlen(info->end_str);
	
	/* search the path */
	list_directory(dir, collect_callback, info);	
}

static int collect(lua_State *L, int flags)
{
	int n = lua_gettop(L);
	int i;
	COLLECT_CALLBACK_INFO info;
	
	if(n < 1)
		luaL_error(L, "collect: incorrect number of arguments");

	/* create the table */
	lua_newtable(L);

	/* set common info */		
	info.lua = L;
	info.i = 1;
	info.flags = flags;

	/* start processing the input strings */
	for(i = 1; i <= n; i++)
	{
		const char *input = lua_tostring(L, i);
		
		if(!input)
			continue;
			
		run_collect(&info, input);
	}
	
	return 1;
}

int lf_collect(lua_State *L) { return collect(L, COLLECTFLAG_FILES); }
int lf_collectrecursive(lua_State *L) { return collect(L, COLLECTFLAG_FILES|COLLECTFLAG_RECURSIVE); }
int lf_collectdirs(lua_State *L) { return collect(L, COLLECTFLAG_DIRS); }
int lf_collectdirsrecursive(lua_State *L) { return collect(L, COLLECTFLAG_DIRS|COLLECTFLAG_RECURSIVE); }
