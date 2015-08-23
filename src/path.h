
#include "platform.h"

/* max path length for bam */
#define MAX_PATH_LENGTH 1024

/* make sure that this gets inlined */
#if defined(BAM_FAMILY_WINDOWS)
	#define PATH_SEPARATOR ('/')
	#define path_is_separator(c) (c == PATH_SEPARATOR || c == '\\')
#else
	#define PATH_SEPARATOR ('/')
	#define path_is_separator(c) (c == PATH_SEPARATOR)
#endif

/* returns a pointer to the filename, /foo/bar.a -> bar.a */
extern const char *path_filename(const char *path);

/*  /foo/bar.a -> /foo */
extern int path_directory(const char *path, char *directory, int size);

/* returns a pointer to where the extention starts, or empty string if there is none */
extern const char *path_ext(const char *filename);

/* normalizes a path, rewrites the path */
extern int path_normalize(char *path);

/* joins to paths together and normalizes them. returns 0 on success */
extern int path_join(const char *base, int base_len, const char *extend, int extend_len, char *output, int size);

/* returns 1 if the path is absolute, else it returns 0 */
extern int path_isabs(const char *path);

/* checks so that the path is nice */
/* no /.. /./ or // */
/* must begin with / (absolute) */
/* does not end with / */
extern int path_isnice(const char *path);
