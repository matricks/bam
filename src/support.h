#ifndef FILE_SUPPORT_H
#define FILE_SUPPORT_H

#include <time.h> /* time_t */

#define BAM_STAT_THREAD_LOCKING 0
#define BAM_STAT_THREAD_LEGACY 1
#define BAM_STAT_THREAD_ATOMIC 2

#ifndef BAM_STAT_THREAD_MODE
#define BAM_STAT_THREAD_MODE BAM_STAT_THREAD_LOCKING
#endif

#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_LOCKING
#define BAM_STAT_THREAD_NAME "locking"
#elif BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_LEGACY
#define BAM_STAT_THREAD_NAME "legacy"
#elif BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_ATOMIC
#define BAM_STAT_THREAD_NAME "atomic"
#if defined(__GNUC__) || defined(__clang__)
#if !defined(__ATOMIC_ACQUIRE) || __GCC_ATOMIC_POINTER_LOCK_FREE != 2 || __GCC_ATOMIC_LONG_LOCK_FREE != 2
#error atomic stat thread requires always-lock-free pointer and long operations
#endif
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64) || defined(_M_ARM64))
/* Interlocked operations provide stronger ordering than acquire/release. */
#else
#error atomic stat thread is unsupported by this compiler or target
#endif
#else
#error invalid BAM_STAT_THREAD_MODE: expected 0 (locking), 1 (legacy), or 2 (atomic)
#endif

#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_ATOMIC
struct NODE;
struct ATOMIC_NODE { struct NODE * volatile value; };
struct ATOMIC_FLAG { volatile long value; };
void atomic_node_init(struct ATOMIC_NODE *ptr);
struct NODE *atomic_node_load(struct ATOMIC_NODE *ptr);
void atomic_node_store(struct ATOMIC_NODE *ptr, struct NODE *value);
void atomic_flag_init(struct ATOMIC_FLAG *ptr);
int atomic_flag_load(struct ATOMIC_FLAG *ptr);
void atomic_flag_set(struct ATOMIC_FLAG *ptr);
#endif

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
#elif defined(_AIX)
	#define sync_barrier() __lwsync()
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
	#include <mbarrier.h>
	#define sync_barrier() __compiler_barrier()
#elif defined(__hpux)
	#define sync_barrier() _Asm_mf()
#elif defined(_MSC_VER)
	#include <intrin.h>
	#define sync_barrier() _ReadWriteBarrier()
#else
	#error missing atomic implementation for this compiler
#endif

struct lua_State;
struct HEAP;

/* common list if strings structure */
struct STRINGLIST
{
	struct STRINGLIST *next;
	const char *str;
	size_t len;
};

/* */
void install_signals(void (*abortsignal)(int));
int run_command(const char *cmd, const char *filter);

void platform_init();
void platform_shutdown();

/* threading */
void *threads_create(void (*threadfunc)(void *), void *u);
void threads_join(void *thread);
#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_LOCKING
void threads_signal_work(void (*change_state)(void *user), void *user);
void threads_signal_wait(int (*is_ready)(void *user), void *user);
#else
void threads_yield();
#endif
void threads_sleep(int milliseconds);
int threads_corecount();

void criticalsection_enter();
void criticalsection_leave();

/* time */
int64 time_get();
int64 time_freq();

/* filesystem and timestamps */
time_t timestamp();
time_t file_timestamp(const char *filename);
int file_isregular(const char *path);
int file_isdir(const char *path);
int file_stat(const char *filename, time_t* stamp, unsigned int* isregular, unsigned int* isdir); 
int file_createdir(const char *path);
int file_createpath(const char *output_name);
void file_touch(const char *filename);
void file_listdirectory(const char *path, void (*callback)(const char *fullpath, const char *filename, int dir, void *user), void *user);

/* string helper functions */
char *string_duplicate(struct HEAP *heap, const char *src, size_t len);
int string_compare_case_insensitive( const char* str_a, const char* str_b );

/* string hashing function */
hash_t string_hash_path(const char *str_in);
hash_t string_hash_path_add(hash_t base, const char *str_in);

hash_t string_hash_djb2(const char *str_in);
hash_t string_hash_djb2_add(hash_t base, const char *str_in);

void string_hash_tostr(hash_t value, char *output);

/* logging */
void event_begin(int thread, const char *name, const char *data);
void event_end(int thread, const char *name, const char *data);

#endif
