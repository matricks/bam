#include <string.h> /* memset */
#include <stdlib.h> /* malloc */
#include <stdio.h> /* printf */

#include "cache.h"
#include "context.h"
#include "node.h"
#include "platform.h"
#include "session.h"
#include "support.h"

/* buffer sizes */
#define WRITE_BUFFERSIZE (32*1024)
#define WRITE_BUFFERNODES (WRITE_BUFFERSIZE/sizeof(struct CACHENODE))
#define WRITE_BUFFERDEPS (WRITE_BUFFERSIZE/sizeof(unsigned))


/* header info */
static char bamheader[8] = {
	'B','A','M',0, /* signature */
	sizeof(void*), /* pointer size */
	0,0,0           /* version hash */
};

static void cache_setup_header()
{
	/* save a hashed version of the date and time in the header */
	unsigned hash = string_hash(__DATE__ __TIME__);
	bamheader[5] = hash&0xff;
	bamheader[6] = (hash>>8)&0xff;
	bamheader[7] = (hash>>16)&0xff;
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

static int cachenode_cmp(struct CACHENODE *a, struct CACHENODE *b)
{
	if(a->hashid > b->hashid) return 1;
	if(a->hashid < b->hashid) return -1;
	return 0;
}

RB_HEAD(CACHENODERB, CACHENODE);
RB_GENERATE_INTERNAL(CACHENODERB, CACHENODE, rbentry, cachenode_cmp, static)

void CACHENODE_FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
{
	(void)CACHENODERB_RB_REMOVE; (void)CACHENODERB_RB_NFIND; (void)CACHENODERB_RB_MINMAX;
	(void)CACHENODERB_RB_NEXT; (void)CACHENODERB_RB_PREV;
}

struct CACHE
{
	char header[sizeof(bamheader)];
	
	unsigned num_nodes;
	unsigned num_deps;
	
	struct CACHENODERB nodetree;
	
	struct CACHENODE *nodes;
	unsigned *deps;
	char *strings;
};
	
struct WRITEINFO
{
	IO_HANDLE fp;
	struct GRAPH *graph;
	
	union
	{
		struct CACHENODE nodes[WRITE_BUFFERNODES];
		unsigned deps[WRITE_BUFFERDEPS];
		char strings[WRITE_BUFFERSIZE];
	} buffers;
	
	/* index into nodes or deps */	
	unsigned index;
};
#if 0


static int write_header(struct WRITEINFO *info)
{
	/* setup the cache */
	struct CACHE cache;
	memset(&cache, 0, sizeof(struct CACHE));
	memcpy(cache.header, bamheader, sizeof(cache.header));
	cache.num_nodes = info->graph->num_nodes;
	cache.num_deps = info->graph->num_deps;
	if(io_write(info->fp, &cache, sizeof(cache)) != sizeof(cache))
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
		struct CACHENODE *cachenode = &info->buffers.nodes[info->index++];

		/* count dependencies */
		struct NODELINK *dep;
		
		memset(cachenode, 0, sizeof(struct CACHENODE));
		
		cachenode->deps_num = 0;
		for(dep = node->firstdep; dep; dep = dep->next)
			cachenode->deps_num++;
		
		cachenode->hashid = node->hashid;
		cachenode->cmdhash = node->cachehash;
		cachenode->cached = node->cached;
		cachenode->timestamp_raw = node->timestamp_raw;
		cachenode->deps = (unsigned*)((long)dep_index);
		cachenode->filename = (char*)((long)string_index);
		
		string_index += node->filename_len;
		dep_index += cachenode->deps_num;
		
		if(info->index == WRITE_BUFFERNODES && write_flush(info, sizeof(struct CACHENODE)))
			return -1;
	}

	/* flush the remainder */
	if(info->index && write_flush(info, sizeof(struct CACHENODE)))
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

int cache_save(const char *filename, struct GRAPH *graph)
{
	struct WRITEINFO info;
	info.index = 0;
	info.graph = graph;

	info.fp = io_open_write(filename);
	if(!io_valid(info.fp))
		return -1;
	
	cache_setup_header();
	
	if(write_header(&info) || write_nodes(&info))
	{
		/* error occured, trunc the cache file so we don't leave a corrupted file */
		io_close(info.fp);
		io_close(io_open_write(filename));
		return -1;
	}

	/* close up and return */
	io_close(info.fp);
	return 0;
}
#endif


int cache_save(const char *filename, struct GRAPH *graph)
{
	return 0;
}

struct CACHE *cache_load(const char *filename)
{
	unsigned long filesize;
	void *buffer;
	struct CACHE *cache;
	unsigned i;
	size_t bytesread;
	
	IO_HANDLE fp;
	
	/* open file */
	fp = io_open_read(filename);
	if(!io_valid(fp))
		return 0;
		
	/* read the whole file */
	filesize = io_size(fp);

	buffer = malloc(filesize);
	
	bytesread = io_read(fp, buffer, filesize);
	io_close(fp);

	cache_setup_header();
	
	/* verify read and headers */
	cache = (struct CACHE *)buffer;
	
	if(	bytesread != filesize ||
		filesize < sizeof(struct CACHE) ||
		memcmp(cache->header, bamheader, sizeof(bamheader)) != 0 ||
		filesize < sizeof(struct CACHE)+cache->num_nodes*sizeof(struct CACHENODE))
	{
		printf("%s: warning: cache file is invalid, generating new one\n", session.name);
		free(buffer);
		return 0;
	}
	
	/* setup pointers */
	cache->nodes = (struct CACHENODE *)(cache+1);
	cache->deps = (unsigned *)(cache->nodes+cache->num_nodes);
	cache->strings = (char *)(cache->deps+cache->num_deps);
	
	/* build node tree and patch pointers */
	for(i = 0; i < cache->num_nodes; i++)
	{
		cache->nodes[i].filename = cache->strings + (long)cache->nodes[i].filename;
		cache->nodes[i].deps = cache->deps + (long)cache->nodes[i].deps;
		RB_INSERT(CACHENODERB, &cache->nodetree, &cache->nodes[i]);
	}
	
	/* done */
	return cache;
}

struct CACHENODE *cache_find_byindex(struct CACHE *cache, unsigned index)
{
	return &cache->nodes[index];
}

struct CACHENODE *cache_find_byhash(struct CACHE *cache, unsigned hashid)
{
	struct CACHENODE tempnode;
	if(!cache)
		return NULL;
	tempnode.hashid = hashid;
	return RB_FIND(CACHENODERB, &cache->nodetree, &tempnode);
}

int cache_do_dependency(
	struct CONTEXT *context,
	struct NODE *node,
	void (*callback)(struct NODE *node, struct CACHENODE *cachenode, void *user),
	void *user)
{
	struct CACHENODE *cachenode;
	struct CACHENODE *depcachenode;
	int i;
	
	/* search the cache */
	cachenode = cache_find_byhash(context->cache, node->hashid);
	if(cachenode && cachenode->cached && cachenode->timestamp_raw == node->timestamp_raw)
	{
		if(node->depchecked)
			return 1;

		node->depchecked = 1;
		
		/* use cached version */
		for(i = cachenode->deps_num-1; i >= 0; i--)
		{
			depcachenode = cache_find_byindex(context->cache, cachenode->deps[i]);
			if(depcachenode->cached)
				callback(node, depcachenode, user);
		}
		
		return 1;
	}
	
	return 0;
}
