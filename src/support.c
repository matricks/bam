#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <time.h>


#include "platform.h"
#include "path.h"
#include "context.h"

#ifdef BAM_FAMILY_BEOS
    #include <sched.h>
#endif

#ifdef BAM_FAMILY_WINDOWS
	#define WIN32_LEAN_AND_MEAN
	#define VC_EXTRALEAN
	#include <windows.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <signal.h>

	/*#define _stat stat*/
	
	/* Windows code */

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
	}

	void platform_shutdown()
	{
		DeleteCriticalSection(&criticalsection);
	}

	void criticalsection_enter()
	{
		EnterCriticalSection(&criticalsection);
	}

	void criticalsection_leave()
	{
		LeaveCriticalSection(&criticalsection);
	}

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

#else
	#include <dirent.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/signal.h>
	#include <sys/stat.h>
	#include <pthread.h>

	static void list_directory(const char *path, void (*callback)(const char *filename, int dir, void *user), void *user)
	{
		DIR *dir;
		struct dirent *entry;
		struct stat info;
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
			
			/* push the string and continue the search */
			stat(buffer, &info);
			
			if(S_ISDIR(info.st_mode))
				callback(buffer, 1, user);
			else
				callback(buffer, 0, user);
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

	void platform_init()
	{
	}

	void platform_shutdown()
	{
	}

	void criticalsection_enter()
	{
		pthread_mutex_lock(&lock_mutex);
	}

	void criticalsection_leave()
	{
		pthread_mutex_unlock(&lock_mutex);
	}

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
#endif

time_t timestamp() { return time(NULL); }
	
#ifdef BAM_PLATFORM_MACOSX
	/* Mac OS X version */
	time_t file_timestamp(const char *filename)
	{
		struct stat s;
		if(stat(filename, &s) == 0)
			return s.st_mtimespec.tv_sec;
		return 0;
	}
#else
	/* NIX and Windows version */
	time_t file_timestamp(const char *filename)
	{
		struct stat s;
		if(stat(filename, &s) == 0)
			return s.st_mtime;
		return 0;
	}
#endif

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
		path_join(context_get_path(L), lua_tostring(L,1), buffer, sizeof(buffer));
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

	/* check end */
	if(info->end_len > no_pathed_len || strcmp(no_pathed+no_pathed_len-info->end_len, info->end_str))
		return;
	
	/* check start */		
	if(info->start_len && strncmp(no_pathed, info->start_str, info->start_len))
		return;
		
	if((dir && info->flags&COLLECTFLAG_DIRS) || (!dir && info->flags&COLLECTFLAG_FILES))
	{
		/* accepted, push the result */
		lua_pushstring(info->lua, filename);
		lua_rawseti(info->lua, -2, info->i++);
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
	{
		lua_pushstring(L, "collect: incorrect number of arguments");
		lua_error(L);
	}

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
