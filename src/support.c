#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>

#include "platform.h"
#include "path.h"
#include "context.h"
#include "session.h"
#include "support.h"
#include "mem.h"

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

	#include <aclapi.h> /* for protect_process */
	
	void file_listdirectory(const char *path, void (*callback)(const char *fullpath, const char *filename, int dir, void *user), void *user)
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
				callback(buffer, finddata.cFileName, 1, user);
			else
				callback(buffer, finddata.cFileName, 0, user);
		} while (FindNextFileA(handle, &finddata));

		FindClose(handle);
	}

	static HANDLE singleton_mutex = 0;
	static DWORD observe_pid = 0;
	static void observer_thread(void *u)
	{
		HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, observe_pid);
		DWORD errcode = -1;

		while(1) {
			WaitForSingleObject(process, INFINITE);
			if(GetExitCodeProcess(process, &errcode) != 0) {
				if(errcode != STILL_ACTIVE) {
					printf("observed process %ld died, aborting!\n", observe_pid);
					session.abort = 1;
					break;
				}
			}
			Sleep(100);
		}

		CloseHandle(process);
	}

	/* signals. should be moved to platform.c or similar? */
	void install_signals(void (*abortsignal)(int))
	{
		/* abortsignal_func = abortsignal ;*/
		signal(SIGINT, abortsignal);
		signal(SIGBREAK, abortsignal);
	}

	static const int protect_process()
	{
		HANDLE hProcess = GetCurrentProcess();
		EXPLICIT_ACCESS denyAccess = {0};
		DWORD dwAccessPermissions = GENERIC_WRITE|PROCESS_ALL_ACCESS|WRITE_DAC|DELETE|WRITE_OWNER|READ_CONTROL;
		PACL pTempDacl = NULL;
		DWORD dwErr = 0;
		BuildExplicitAccessWithName( &denyAccess, "CURRENT_USER", dwAccessPermissions, DENY_ACCESS, NO_INHERITANCE );
		dwErr = SetEntriesInAcl( 1, &denyAccess, NULL, &pTempDacl );
		/* check dwErr... */
		dwErr = SetSecurityInfo( hProcess, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, pTempDacl, NULL );
		/* check dwErr... */
		LocalFree( pTempDacl );
		CloseHandle( hProcess );
		return dwErr == ERROR_SUCCESS;
	}

	static CRITICAL_SECTION criticalsection;


/* #define BAM_USE_JOBOBJECT */

#ifdef BAM_USE_JOBOBJECT
	static HANDLE jobhandle;
#endif

	void platform_init()
	{
		char buffer[512] = {0};

		/* this environment variable is set by Microsoft Visual Studio
			when building. It causes cl.exe to redirect it's output to
			the specified pipe id. this causes loads of problems with
			output.
		*/ 
		SetEnvironmentVariable("VS_UNICODE_OUTPUT", NULL);

		/* check if we are being spawned by bam in msvc mode so we should be singleton */
		if(GetEnvironmentVariable("BAM_SINGLETON", buffer, sizeof(buffer)-1))
		{
			DWORD ret;
			SetEnvironmentVariable("BAM_SINGLETON", NULL);

			singleton_mutex = CreateMutex(NULL, FALSE, "Global\\bam_singleton_mutex");
			if(!singleton_mutex)
			{
				printf("bam is already running, wait for it to finish and then try again 1\n");
				exit(1);
			}

			while(1)
			{
				ret = WaitForSingleObject( singleton_mutex, 100 );
				if(ret == WAIT_OBJECT_0)
					break;
				else
				{
					/* printf("bam is already running, wait for it to finish and then try again 2 (%d)\n", ret); */
					printf("bam is already running, waiting for it to finish\n"); fflush(stdout);
					Sleep(1000);
				}
			}
		}

		/* this environment variable can be setup so that bam can observe if
			a specific process dies and abort the building if it does. This
			is used in conjunction with Microsoft Visual Studio to make sure
			that it doesn't kill processes wildly.
		*/
		if(GetEnvironmentVariable("BAM_OBSERVE_PID", buffer, sizeof(buffer)-1))
		{
			observe_pid = atoi(buffer);
			threads_create(observer_thread, NULL);

			/* protect process from being killed */
			protect_process();
		}
		else
		{
			if( session.win_msvcmode )
			{
				DWORD errcode;
				PROCESS_INFORMATION pi;
				STARTUPINFO si;

				/* setup ourself to be watched */
				sprintf(buffer, "%ld", GetCurrentProcessId());
				SetEnvironmentVariable("BAM_OBSERVE_PID", buffer);

				/* signal that we want to be singleton */
				SetEnvironmentVariable("BAM_SINGLETON", "1");

				/* init structs and create process */
				ZeroMemory( &si, sizeof(si) );
				si.cb = sizeof(si);
				ZeroMemory( &pi, sizeof(pi) );

				if( CreateProcess(
					NULL,      /* No module name (use command line) */
					GetCommandLine(),   /* Command line */
					NULL,      /* Process handle not inheritable */
					NULL,      /* Thread handle not inheritable */
					TRUE,      /* Set handle inheritance to FALSE */
					0,         /* No creation flags */
					NULL,      /* Use parent's environment block */
					NULL,      /* Use parent's starting directory */
					&si,       /* Pointer to STARTUPINFO structure */
					&pi        /* Pointer to PROCESS_INFORMATION structure */
				) ) {
					/* wait for the child to complete */
					WaitForSingleObject(pi.hProcess, INFINITE);
					GetExitCodeProcess(pi.hProcess, &errcode);
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
					exit(errcode);
				} else {
					printf("failed to spawn new bam process in msvc mode\n");
					printf("%s\n", GetCommandLine());
					exit(1);
				}
			} else {
#ifdef BAM_USE_JOBOBJECT
				jobhandle = CreateJobObjectA( sec, NULL );
				AssignProcessToJobObject(jobhandle, GetCurrentProcess());

				JOBOBJECT_BASIC_LIMIT_INFORMATION limitinfo = { 0 };
				limitinfo.limitflags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				SetInformationJobObject(jobhandle, JobObjectBasicLimitInformation, &limitinfo, sizeof(limitinfo));
#endif
			}
		}

		InitializeCriticalSection(&criticalsection);
	}
	
	void platform_shutdown()
	{
#ifdef BAM_USE_JOBOBJECT
		/* Should we really close the handle here? */
		/* CloseHandle(jobhandle); */
#endif
		if(singleton_mutex)
			ReleaseMutex(singleton_mutex);
		CloseHandle(singleton_mutex);
		DeleteCriticalSection(&criticalsection);
	}

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


	void threads_sleep(int milliseconds)
	{
		Sleep(milliseconds);
	}
	
	int threads_corecount()
	{
		SYSTEM_INFO sysinfo;
		GetSystemInfo(&sysinfo);
		if(sysinfo.dwNumberOfProcessors >= 1)
			return sysinfo.dwNumberOfProcessors;
		return 1;
	}


	int64 time_get()
	{
		static int64 last = 0;
		int64 t;
		QueryPerformanceCounter((PLARGE_INTEGER)&t);
		if(t<last) /* for some reason, QPC can return values in the past */
			return last;
		last = t;
		return t;
	}

	int64 time_freq()
	{
		int64 t;
		QueryPerformanceFrequency((PLARGE_INTEGER)&t);
		return t;
	}

#else
	#define D_TYPE_HACK

#ifdef D_TYPE_HACK
	#define __USE_BSD
#endif

	#include <dirent.h>
	#include <dlfcn.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <sys/wait.h> 
	#include <utime.h>
	#include <pthread.h>

	#include <sys/time.h>

#ifdef BAM_PLATFORM_MACOSX
	#include <sys/param.h>
	#include <sys/sysctl.h>
#endif

#ifdef BAM_FAMILY_BEOS
	#include <signal.h>
#else
	#include <sys/signal.h>
#endif

/* disable D_TYPE_HACK if we don't have support for it */
#if !defined(_DIRENT_HAVE_D_TYPE) || !defined(DT_DIR) || !defined(DT_UNKNOWN)
	#undef D_TYPE_HACK
#endif

	void file_listdirectory(const char *path, void (*callback)(const char *fullpath, const char *filename, int dir, void *user), void *user)
	{
		struct dirent **namelist;
		int n;
		struct dirent *entry;
		struct stat info;
		char buffer[1024];
		char *startpoint;
		
		if(*path == 0) /* special case for current directory */
		{
			startpoint = buffer;
			strcpy(buffer, ".");
		}
		else
		{
			/* get starting point and append a slash */
			strcpy(buffer, path);
			startpoint = buffer + strlen(buffer);
			*(startpoint++) = '/';
			*startpoint = 0;
		}
		
		n = scandir(buffer, &namelist, NULL, alphasort);
		if(n == -1)
			return;
		
		while(n--)
		{
			int isdir;
			entry = namelist[n];
			/* make the path absolute */
			strcpy(startpoint, entry->d_name);
#ifdef D_TYPE_HACK
			if(entry->d_type != DT_UNKNOWN)
			{
				isdir = (entry->d_type == DT_DIR);
			}
			else
			{
				/* do stat to obtain if it's a directory or not */
				stat(buffer, &info);
				isdir = S_ISDIR(info.st_mode);
			}
#else
			/* do stat to obtain if it's a directory or not */
			stat(buffer, &info);
			isdir = S_ISDIR(info.st_mode);
#endif
			free(entry);
			/* call the callback */
			callback(buffer, entry->d_name, isdir, user);
		}
		
		free(namelist);
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

	void threads_sleep(int milliseconds)
	{
		usleep(milliseconds*1000);
	}

	int threads_corecount()
	{
#ifdef BAM_PLATFORM_MACOSX
		int nm[2] = {CTL_HW, HW_AVAILCPU};
		size_t len = 4;
		uint32_t count;

		sysctl(nm, 2, &count, &len, NULL, 0);

		if(count < 1)
		{
			nm[1] = HW_NCPU;
			sysctl(nm, 2, &count, &len, NULL, 0);
			if(count < 1) { count = 1; }
		}
		if(count >= 1)
			return count;
		return 1;
#elif defined(BAM_PLATFORM_HPUX)
#include <sys/pstat.h>
	struct pst_dynamic psd;

	if (!pstat_getdynamic(&psd, sizeof(psd), (size_t)1, 0))
	{
		return psd.psd_proc_cnt;
	}
	return 1;
#else
	    int count = sysconf(_SC_NPROCESSORS_ONLN);
	    if(count >= 1)
	    	return count;
	    return 1;
#endif
	}

	int64 time_get()
	{
		struct timeval val;
		gettimeofday(&val, NULL);
		return (int64)val.tv_sec*(int64)1000000+(int64)val.tv_usec;
	}

	int64 time_freq()
	{
		return 1000000;
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

int file_isregular(const char *filename)
{
#ifdef BAM_FAMILY_WINDOWS
	struct stat s;
	if(stat(filename, &s) == 0)
	{
		if (s.st_mode&_S_IFREG)
			return 1;
	}
#else
	struct stat s;
	if(stat(filename, &s) == 0)
		return S_ISREG(s.st_mode);
#endif
	return 0;
}


int file_isdir(const char *filename)
{
#ifdef BAM_FAMILY_WINDOWS
	struct stat s;
	if(stat(filename, &s) == 0)
	{
		if (s.st_mode&_S_IFDIR)
			return 1;
	}
#else
	struct stat s;
	if(stat(filename, &s) == 0)
		return S_ISDIR(s.st_mode);
#endif
	return 0;
}

int file_stat(const char *filename, time_t* stamp, unsigned int* isregular, unsigned int* isdir)
{
	struct stat s;
	if (stat(filename, &s) != 0)
	{
		*stamp = 0;
		*isregular = 0;
		*isdir = 0;
		return 1;
	}
#ifdef BAM_FAMILY_WINDOWS
	*stamp = s.st_mtime;
	*isregular = (s.st_mode&_S_IFREG) != 0;
	*isdir = (s.st_mode&_S_IFDIR) != 0;
#elif defined(BAM_PLATFORM_MACOSX)
	*stamp = s.st_mtimespec.tv_sec;
	*isregular = S_ISREG(s.st_mode);
	*isdir = S_ISDIR(s.st_mode);
#else
	*stamp = s.st_mtime;
	*isregular = S_ISREG(s.st_mode);
	*isdir = S_ISDIR(s.st_mode);
#endif
	return 0;
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
	/*
		_utime under windows seem to contain a bug that doesn't release the file handle in a timly fashion.
		This implementation is basiclly the same but smaller and less cruft.
	*/
	HANDLE handle;
	FILETIME ft;
	SYSTEMTIME st;

	handle = CreateFile(
		filename,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if(handle != INVALID_HANDLE_VALUE)
	{
		GetSystemTime(&st);
		SystemTimeToFileTime(&st, &ft);
		SetFileTime(handle, (LPFILETIME)NULL, (LPFILETIME)NULL, &ft);
		CloseHandle(handle);
	}
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

#if defined(BAM_FAMILY_WINDOWS) || defined(BAM_PLATFORM_CYGWIN)
/* forward declaration */
FILE *_popen(const char *, const char *);
int _pclose(FILE *);
#endif

int run_command(const char *cmd, const char *filter)
{
	int ret;
	
#ifdef BAM_FAMILY_WINDOWS
	/* windows has a buggy command line parser. I takes the first and
	last '"' and removes them which causes problems in cases like this:
	
		"C:\t t\test.bat" 1 2 "3" 4 5
		
	Which will yeild:

		C:\t t\test.bat" 1 2 "3 4 5
		
	The work around is to encase the command line with '"' that it can remove.
	
	*/
	char buf[32*1024];
	FILE *fp;
	size_t len = strlen(cmd);
	if(len > sizeof(buf)-3) /* 2 for '"' and 1 for '\0' */
		return -1;
	buf[0] = '"';
	memcpy(buf+1, cmd, len);
	buf[len+1] = '"';
	buf[len+2] = 0;
	
	/* open the command line */
	fp = _popen(buf, "r");

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
	if(WIFSIGNALED(ret))
		raise(SIGINT);
#endif
	if(session.verbose)
		printf("%s: ret=%d %s\n", session.name, ret, cmd);
	return ret;
}

/* like file_createdir, but automatically creates all top-level directories needed
	If you feed it "output/somefiles/output.o" it will create "output/somefiles"
*/	
int file_createpath(const char *output_name)
{
	char buffer[MAX_PATH_LENGTH];
	int i;
	char t;
	
	/* fish out the directory */
	if(path_directory(output_name, buffer, sizeof(buffer)) != 0)
	{
		fprintf(stderr, "path error: %s\n", buffer);
		return -1;
	}
	
	/* no directory in path */
	if(buffer[0] == 0)
		return 0;
	
	/* check if we need to do a deep walk */
	if(file_createdir(buffer) == 0)
		return 0;
	
	/* create dir by doing a deep walk */
	i = 1; /* we need at least one character, or we'll try to create "" if the path is unix absolute (/path/to)*/
	while(1)
	{
		if(path_is_separator(buffer[i]) || (buffer[i] == 0))
		{
			/* insert null terminator */
			t = buffer[i];
			buffer[i] = 0;
			
			if(file_createdir(buffer) != 0)
			{
				fprintf(stderr, "path error2: %s\n", buffer);
				return -1;
			}
			
			/* restore the path */
			buffer[i] = t;
		}
		
		if(buffer[i] == 0)
			break;
		
		i++;
	}
	
	/* return success */
	return 0;
}

/* */
char *string_duplicate(struct HEAP *heap, const char *src, size_t len)
{
	char *str = (char *)mem_allocate(heap, len+1);
	memcpy(str, src, len);
	str[len] = 0;
	return str;
}

/* */
/* on windows, we need to handle that filenames with mixed casing are the same.
	to solve this we have this table that converts all uppercase letters.
	in addition to this, we also convert all '\' to '/' to remove that
	ambiguity
*/
static const unsigned char tolower_table[256] = {
0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
64, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 91, '/', 93, 94, 95,
96, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 123, 124, 125, 126, 127,
128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};

#ifdef BAM_FAMILY_WINDOWS
hash_t string_hash_add(hash_t h, const char *str_in)
{
	const unsigned char *str = (const unsigned char *)str_in;
	for (; *str; str++)
		h = (33*h) ^ tolower_table[*str];
	return h;
}
#else
/* normal unix version */
hash_t string_hash_add(hash_t h, const char *str)
{
	for (; *str; str++)
		h = (33*h) ^ *str;
	return h;
}
#endif

hash_t string_hash(const char *str_in)
{
	return string_hash_add(5381, str_in);
}

void string_hash_tostr(hash_t value, char *output)
{
	sprintf(output, "%08x%08x", (unsigned)(value>>32), (unsigned)(value&0xffffffff));
}

int string_compare_case_insensitive( const char* str_a, const char* str_b )
{
	int d;
	for(; *str_a && *str_b; str_a++, str_b++ )
	{
		d = (int)tolower_table[(int)*(str_a)] - (int)tolower_table[(int)*(str_b)];
		if(d != 0)
			return d;
	};
	return (int)*str_a - (int)*str_b; // we are just comparing 0/ so no decasing necessary
}

static int64 starttime = 0;

static void event_log(int thread, const char *type, const char *name, const char *data)
{
	double t;
	if(session.eventlog == NULL)
		return;

	if(starttime == 0)
		starttime = time_get();

	if(data == NULL)
		data = "";

	t = (time_get() - starttime) / (double)time_freq();
	fprintf(session.eventlog, "%d %f %s %s: %s\n", thread, t, type, name, data);

	if(session.eventlogflush)
		fflush(session.eventlog);
}

void event_begin(int thread, const char *name, const char *data)
{
	event_log(thread, "begin", name, data);
}

void event_end(int thread, const char *name, const char *data)
{
	event_log(thread, "end", name, data);
}
