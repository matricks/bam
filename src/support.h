#ifndef FILE_SUPPORT_H
#define FILE_SUPPORT_H

#include <time.h> /* time_t */

/* types */ 
#ifdef __GNUC__
	/* if compiled with -pedantic-errors it will complain about long long not being a C90 thing. */
	__extension__ typedef unsigned long long hash_t;
	__extension__ typedef long long int64;
#else
	typedef unsigned long long hash_t;
	typedef long long int64;
#endif

#if defined(__GNUC__)
	#define sync_barrier() __sync_synchronize()
#elif defined(_MSC_VER)
	#include <intrin.h>
	#define sync_barrier() _ReadWriteBarrier()
#else
	#error missing atomic implementation for this compiler
#endif
struct lua_State;

void install_signals(void (*abortsignal)(int));
int run_command(const char *cmd, const char *filter);

void platform_init();
void platform_shutdown();

/* threading */
void *threads_create(void (*threadfunc)(void *), void *u);
void threads_join(void *thread);
void threads_yield();
int threads_corecount();

void criticalsection_enter();
void criticalsection_leave();

/* time */
int64 time_get();
int64 time_freq();

/* filesystem and timestamps */
time_t timestamp();
time_t file_timestamp(const char *filename);
int file_createdir(const char *path);
int file_createpath(const char *output_name);
void file_touch(const char *filename);

/* string hashing function */
hash_t string_hash(const char *str_in);
hash_t string_hash_add(hash_t base, const char *str_in);
void string_hash_tostr(hash_t value, char *output);

/* logging */
void event_begin(int thread, const char *name, const char *data);
void event_end(int thread, const char *name, const char *data);

#endif
