#include <string.h> /* memset */
#include <stdlib.h> /* malloc */
#include <stdio.h> /* printf */

#include "cache.h"
#include "context.h"
#include "node.h"
#include "platform.h"
#include "session.h"
#include "support.h"

#include "version.h"

/* buffer sizes */
#define WRITE_BUFFERSIZE (32*1024)
#define WRITE_BUFFERNODES (WRITE_BUFFERSIZE/sizeof(struct CACHEINFO_DEPS))
#define WRITE_BUFFERDEPS (WRITE_BUFFERSIZE/sizeof(unsigned))

/* increase this by one if changes to the cache format have been done */
#define CACHE_VERSION	1

/* header info */
static char bamheader[24] = {
	'B','A','M',0,					/* signature */

	0,0,0,0,						/* cache type */

	0,0,0,0,0,0,0,0,				/* bam version string goes here */

	CACHE_VERSION,					/* cache version */
	sizeof(void*),					/* pointer size */
	sizeof(struct CACHEINFO_DEPS),	/* deps cache info */
	sizeof(struct CACHEINFO_OUTPUT),/* output cache info */

	0,0,0,0 						/* byte order mark */
};

static void cache_setup_header(const char type[4])
{
	unsigned byteordermark = 0x12345678;
	memcpy(&bamheader[4], type, 3);
	memcpy(&bamheader[8], BAM_VERSION_STRING_COMPLETE, sizeof(BAM_VERSION_STRING_COMPLETE));
	bamheader[20] = ((char*)&byteordermark)[0];
	bamheader[21] = ((char*)&byteordermark)[1];
	bamheader[22] = ((char*)&byteordermark)[2];
	bamheader[23] = ((char*)&byteordermark)[3];
}

/* 	detect if we can use unix styled io. we do this because fwrite
	can use it's own buffers and bam already to it's buffering nicely
	so this will reduce the number of syscalls needed. */
#ifdef BAM_FAMILY_UNIX
	#include <fcntl.h>
	#if defined(O_RDONLY) && defined(O_WRONLY) && defined(O_CREAT) && defined(O_TRUNC)
		#define USE_UNIX_IO
	#endif
#endif

/* setup io */
#ifdef USE_UNIX_IO
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>

	#define IO_HANDLE int
	#define io_open_read(filename) open(filename, O_RDONLY)
	#define io_open_write(filename) open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666)
	#define io_valid(f) ((f) != -1)
	#define io_close(f) close(f)
	#define io_read(f, data, size) read(f, data, size)
	#define io_write(f, data, size) write(f, data, size)
	
	size_t io_size(IO_HANDLE f)
	{
		struct stat s;
		if(!fstat(f, &s))
			return s.st_size;
		else
		{
			perror("fstat");
			return 0;
		}
	}
	
#else
	#include <stdio.h> /* FILE, f* */

	#define IO_HANDLE FILE*
	#define io_open_read(filename) fopen(filename, "rb")
	#define io_open_write(filename) fopen(filename, "wb")
	#define io_valid(f) (f)
	#define io_close(f) fclose(f)
	#define io_read(f, data, size) fread(data, 1, size, f)
	#define io_write(f, data, size) fwrite(data, 1, size, f)

	size_t io_size(IO_HANDLE f)
	{
		long current, end;
		current = ftell(f);
		fseek(f, 0, SEEK_END);
		end = ftell(f);
		fseek(f, current, SEEK_SET);
		return end;
	}
#endif

static int io_read_cachefile(const char *filename, const char *type, void **buffer, unsigned long *buffersize)
{
	unsigned long filesize;
	IO_HANDLE fp;
	
	/* open file */
	fp = io_open_read(filename);
	if(!io_valid(fp))
		return 0;
		
	/* read the whole file */
	filesize = io_size(fp);
	*buffer = malloc(filesize);
	*buffersize = io_read(fp, *buffer, filesize);
	io_close(fp);

	/* verify read and header */
	cache_setup_header(type);
	if(	*buffersize != filesize ||
		filesize < sizeof(bamheader) ||
		memcmp(*buffer, bamheader, sizeof(bamheader)) != 0)
	{
		printf("%s: warning: cache file '%s' is invalid, generating new one\n", session.name, filename);
		free(*buffer);
		*buffer = NULL;
		return 0;
	}

	return 1;
}


static int cacheinfo_deps_cmp(struct CACHEINFO_DEPS *a, struct CACHEINFO_DEPS *b)
{
	if(a->hashid > b->hashid) return 1;
	if(a->hashid < b->hashid) return -1;
	return 0;
}

RB_HEAD(CACHEINFO_DEPS_RB, CACHEINFO_DEPS);
RB_GENERATE_INTERNAL(CACHEINFO_DEPS_RB, CACHEINFO_DEPS, rbentry, cacheinfo_deps_cmp, static)

void CACHEINFO_DEPS_FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
{
	(void)CACHEINFO_DEPS_RB_RB_REMOVE; (void)CACHEINFO_DEPS_RB_RB_NFIND; (void)CACHEINFO_DEPS_RB_RB_MINMAX;
	(void)CACHEINFO_DEPS_RB_RB_NEXT; (void)CACHEINFO_DEPS_RB_RB_PREV;
}

struct DEPCACHE
{
	char header[sizeof(bamheader)];
	
	unsigned num_nodes;
	unsigned num_deps;
	
	struct CACHEINFO_DEPS_RB nodetree;
	
	struct CACHEINFO_DEPS *nodes;
	unsigned *deps;
	char *strings;
};
	
struct WRITEINFO
{
	IO_HANDLE fp;
	struct GRAPH *graph;
	
	union
	{
		struct CACHEINFO_DEPS nodes[WRITE_BUFFERNODES];
		unsigned deps[WRITE_BUFFERDEPS];
		char strings[WRITE_BUFFERSIZE];
	} buffers;
	
	/* index into nodes or deps */	
	unsigned index;
};

static int write_header(struct WRITEINFO *info)
{
	/* setup the cache */
	struct DEPCACHE depcache;
	memset(&depcache, 0, sizeof(struct DEPCACHE));
	memcpy(depcache.header, bamheader, sizeof(depcache.header));
	depcache.num_nodes = info->graph->num_nodes;
	depcache.num_deps = info->graph->num_deps;
	if(io_write(info->fp, &depcache, sizeof(depcache)) != sizeof(depcache))
		return -1;
	return 0;
}

static int write_flush(struct WRITEINFO *info, int elementsize)
{
	int size = elementsize*info->index;
	if(io_write(info->fp, info->buffers.nodes, size) != size)
		return -1;
	info->index = 0;
	return 0;
}

static int write_nodes(struct WRITEINFO *info)
{
	unsigned dep_index;
	unsigned string_index;
	
	struct NODE *node;
	struct GRAPH *graph = info->graph;
		
	/* write the cache nodes */	
	dep_index = 0;
	string_index = 0;
	for(node = graph->first; node; node = node->next)
	{
		/* fetch cache node */
		struct CACHEINFO_DEPS *cacheinfo = &info->buffers.nodes[info->index++];

		/* count dependencies */
		struct NODELINK *dep;
		
		memset(cacheinfo, 0, sizeof(struct CACHEINFO_DEPS));
		
		cacheinfo->deps_num = 0;
		for(dep = node->firstdep; dep; dep = dep->next)
			cacheinfo->deps_num++;
		
		cacheinfo->hashid = node->hashid;
		cacheinfo->cached = node->cached;
		cacheinfo->timestamp_raw = node->timestamp_raw;
		cacheinfo->deps = (unsigned*)((long)dep_index);
		cacheinfo->filename = (char*)((long)string_index);
		
		string_index += node->filename_len;
		dep_index += cacheinfo->deps_num;
		
		if(info->index == WRITE_BUFFERNODES && write_flush(info, sizeof(struct CACHEINFO_DEPS)))
			return -1;
	}

	/* flush the remainder */
	if(info->index && write_flush(info, sizeof(struct CACHEINFO_DEPS)))
		return -1;

	/* write the cache nodes deps */
	for(node = graph->first; node; node = node->next)
	{
		struct NODELINK *dep;
		for(dep = node->firstdep; dep; dep = dep->next)
		{
			info->buffers.deps[info->index++] = dep->node->id;
			if(info->index == WRITE_BUFFERDEPS && write_flush(info, sizeof(unsigned)))
				return -1;
		}
	}

	/* flush the remainder */
	if(info->index && write_flush(info, sizeof(unsigned)))
		return -1;
		
	/* write the strings */
	for(node = graph->first; node; node = node->next)
	{
		if(info->index+node->filename_len > sizeof(info->buffers.strings))
		{
			if(write_flush(info, sizeof(char)))
				return -1;
		}
		memcpy(info->buffers.strings + info->index, node->filename, node->filename_len);
		info->index += node->filename_len;
	}	

	/* flush the remainder */
	if(info->index && write_flush(info, sizeof(char)))
		return -1;
		
	return 0;
}

int depcache_save(const char *filename, struct GRAPH *graph)
{
	struct WRITEINFO info;
	info.index = 0;
	info.graph = graph;

	info.fp = io_open_write(filename);
	if(!io_valid(info.fp))
		return -1;
	
	cache_setup_header("DEP");
	
	if(write_header(&info) || write_nodes(&info))
	{
		/* error occured, trunc the cache file so we don't leave a corrupted file */
		printf("%s: warning: error saving cache file '%s', truncating it\n", session.name, filename);
		io_close(info.fp);
		io_close(io_open_write(filename));
		return -1;
	}

	/* close up and return */
	io_close(info.fp);
	return 0;
}

struct DEPCACHE *depcache_load(const char *filename)
{
	unsigned long filesize;
	void *buffer;
	struct DEPCACHE *depcache;
	unsigned i;

	if(!io_read_cachefile(filename, "DEP", &buffer, &filesize))
		return NULL;
	
	/* verify read and headers */
	depcache = (struct DEPCACHE *)buffer;
	
	if(	filesize < sizeof(struct DEPCACHE) ||
		filesize < sizeof(struct DEPCACHE)+depcache->num_nodes*sizeof(struct CACHEINFO_DEPS))
	{
		free(buffer);
		return NULL;
	}
	
	/* setup pointers */
	depcache->nodes = (struct CACHEINFO_DEPS *)(depcache+1);
	depcache->deps = (unsigned *)(depcache->nodes+depcache->num_nodes);
	depcache->strings = (char *)(depcache->deps+depcache->num_deps);
	
	/* build node tree and patch pointers */
	for(i = 0; i < depcache->num_nodes; i++)
	{
		depcache->nodes[i].filename = depcache->strings + (long)depcache->nodes[i].filename;
		depcache->nodes[i].deps = depcache->deps + (long)depcache->nodes[i].deps;
		RB_INSERT(CACHEINFO_DEPS_RB, &depcache->nodetree, &depcache->nodes[i]);
	}
	
	/* done */
	return depcache;
}

void depcache_free(struct DEPCACHE *depcache)
{
	free(depcache);
}

struct CACHEINFO_DEPS *depcache_find_byindex(struct DEPCACHE *depcache, unsigned index)
{
	return &depcache->nodes[index];
}

struct CACHEINFO_DEPS *depcache_find_byhash(struct DEPCACHE *depcache, hash_t hashid)
{
	struct CACHEINFO_DEPS tempinfo;
	if(!depcache)
		return NULL;
	tempinfo.hashid = hashid;
	return RB_FIND(CACHEINFO_DEPS_RB, &depcache->nodetree, &tempinfo);
}

int depcache_do_dependency(
	struct CONTEXT *context,
	struct NODE *node,
	void (*callback)(struct NODE *node, struct CACHEINFO_DEPS *cacheinfo, void *user),
	void *user)
{
	struct CACHEINFO_DEPS *cacheinfo;
	struct CACHEINFO_DEPS *depcacheinfo;
	int i;
	
	/* search the cache */
	cacheinfo = depcache_find_byhash(context->depcache, node->hashid);
	if(cacheinfo && cacheinfo->cached && cacheinfo->timestamp_raw == node->timestamp_raw)
	{
		if(node->depchecked)
			return 1;

		node->depchecked = 1;
		
		/* use cached version */
		for(i = cacheinfo->deps_num-1; i >= 0; i--)
		{
			depcacheinfo = depcache_find_byindex(context->depcache, cacheinfo->deps[i]);
			if(depcacheinfo->cached)
				callback(node, depcacheinfo, user);
		}
		
		return 1;
	}
	
	return 0;
}

struct OUTPUTCACHE
{
	struct CACHEINFO_OUTPUT *info;
	unsigned count;
};

static int output_hash_compare(const void * a, const void * b)
{
	hash_t hash_a = ((const struct CACHEINFO_OUTPUT *)a)->hashid;
	hash_t hash_b = ((const struct CACHEINFO_OUTPUT *)b)->hashid;
	if(hash_a > hash_b) return 1;
	if(hash_a < hash_b) return -1;
	return 0;
}

/* returns the number of errors in the cache */
static unsigned validate_outputcache(struct CACHEINFO_OUTPUT *infos, unsigned count)
{
	unsigned errorcount = 0;
	hash_t lasthash = 0;
	unsigned i;
	if(count > 0)
	{
		for( i = 0; i < count; i++ )
		{
			/* hashid has to be in increasing order */
			if(infos[i].hashid <= lasthash)
				errorcount++;

			/* must have a valid timestamp */
			if(infos[i].timestamp == 0)
				errorcount++;

			lasthash = infos[i].hashid;
		}
	}

	return errorcount;
}

static unsigned outputcache_merge(
	struct CACHEINFO_OUTPUT *old, unsigned oldcount,
	struct CACHEINFO_OUTPUT *new, unsigned newcount,
	struct CACHEINFO_OUTPUT *output)
{
	unsigned curold = 0;
	unsigned curnew = 0;
	unsigned curout = 0;

	/* merge the two lists into a new one */
	while(curold < oldcount && curnew < newcount)
	{
		if(new[curnew].hashid == old[curold].hashid)
		{
			output[curout] = new[curnew];
			curnew++;
			curold++;
			curout++;
		}
		else if(new[curnew].hashid < old[curold].hashid)
			output[curout++] = new[curnew++];
		else
			output[curout++] = old[curold++];
	}

	/* add the remaining ones */
	while(curold < oldcount)
		output[curout++] = old[curold++];

	while(curnew < newcount)
		output[curout++] = new[curnew++];

	return curout;
}

int outputcache_save(const char *filename, struct OUTPUTCACHE *oldcache, struct GRAPH *graph)
{
	IO_HANDLE fp;

	unsigned num_outputs;
	unsigned index;
	size_t output_size;
	struct JOB * job;
	struct NODELINK * link;
	struct CACHEINFO_OUTPUT * output;

	struct CACHEINFO_OUTPUT * final;
	unsigned finalcount;

	fp = io_open_write(filename);
	if(!io_valid(fp))
		return -1;

	cache_setup_header("OUT");

	if(io_write(fp, bamheader, sizeof(bamheader)) != sizeof(bamheader))
		return -1;

	/* count outputs */
	num_outputs = 0;
	for(job = graph->firstjob; job; job = job->next)
		for(link = job->firstoutput; link; link = link->next)
			if(link->node->timestamp_raw)
				num_outputs++;

	output_size = sizeof(struct CACHEINFO_OUTPUT) * num_outputs;
	output = malloc(output_size);
	memset(output, 0, output_size);

	index = 0;
	for(job = graph->firstjob; job; job = job->next)
	{
		for(link = job->firstoutput; link; link = link->next)
		{
			if(link->node->timestamp_raw)
			{
				output[index].hashid = link->node->hashid;
				output[index].cmdhash = link->node->job->cachehash;
				output[index].timestamp = link->node->timestamp_raw;

				index++;
			}
		}
	}

	/* sort the nodes */
	qsort(output, num_outputs, sizeof(struct CACHEINFO_OUTPUT), output_hash_compare);

	if(oldcache)
	{
		/* merge with old one */
		final = malloc(sizeof(struct CACHEINFO_OUTPUT) * (num_outputs + oldcache->count));
		finalcount = outputcache_merge(oldcache->info, oldcache->count, output, num_outputs, final);

		/* replace the output */
		free(output);
		output = final;
		num_outputs = finalcount;
		output_size = finalcount * sizeof(struct CACHEINFO_OUTPUT);
	}

	/* write down to disk */
	if(validate_outputcache(output, num_outputs) || io_write(fp, output, output_size) != output_size)
	{
		/* error occured, trunc the cache file so we don't leave a corrupted file */
		printf("%s: warning: error saving cache file '%s', truncating it\n", session.name, filename);
		io_close(fp);
		io_close(io_open_write(filename));
		free(output);
		return -1;
	}

	/* close up and return */
	free(output);
	io_close(fp);
	return 0;
}

struct OUTPUTCACHE *outputcache_load(const char *filename)
{
	unsigned long filesize;
	unsigned long payloadsize;
	void *buffer;
	struct OUTPUTCACHE *cache;

	if(!io_read_cachefile(filename, "OUT", &buffer, &filesize))
		return NULL;

	payloadsize = filesize - sizeof(bamheader);

	/* check so that everything lines up */
	if(payloadsize % sizeof(struct CACHEINFO_OUTPUT) != 0)
	{
		printf("%s: warning: cache file '%s' is invalid, generating new one\n", session.name, filename);
		free(buffer);
		return NULL;
	}

	/* setup the cache structure */
	cache = (struct OUTPUTCACHE*)malloc(sizeof(struct OUTPUTCACHE));
	cache->info = (struct CACHEINFO_OUTPUT *)((char*)buffer + sizeof(bamheader));
	cache->count = payloadsize /  sizeof(struct CACHEINFO_OUTPUT);

	if(validate_outputcache(cache->info, cache->count))
	{
		printf("%s: warning: cache file '%s' is invalid, generating new one\n", session.name, filename);
		free(buffer);
		return NULL;
	}

	/* done */
	return cache;
}


struct CACHEINFO_OUTPUT *outputcache_find_byhash(struct OUTPUTCACHE *outputcache, hash_t hashid)
{
	/* do a binary search for the hashid */
	unsigned low = 0;
	unsigned high = outputcache->count;
	unsigned index;

	if(!outputcache)
		return NULL;

	while(low < high)
	{
		index = low + (high - low) / 2;
		if(hashid < outputcache->info[index].hashid)
		{
			if(index == 0)
				break;
			else
				high = index - 1;
		}
		else if(hashid > outputcache->info[index].hashid)
			low = index + 1;
		else
			return &outputcache->info[index];
	}

	if(hashid == outputcache->info[low].hashid)
		return &outputcache->info[low];

	return NULL;
}

