#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lua.h>

#include "node.h"
#include "mem.h"
#include "support.h"
#include "path.h"
#include "context.h"

#include "tree.h"

RB_HEAD(NODERB, NODE);

static int node_cmp(struct NODE *a, struct NODE *b)
{
	if(a->hashid > b->hashid) return 1;
	if(a->hashid < b->hashid) return -1;
	return 0;
}

RB_GENERATE_INTERNAL(NODERB, NODE, rbentry, node_cmp, static)

void NODE_FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
{
	(void)NODERB_RB_REMOVE;
	(void)NODERB_RB_NFIND;
	(void)NODERB_RB_NEXT;
	(void)NODERB_RB_PREV;
	(void)NODERB_RB_MINMAX;
}

/**/
struct GRAPH
{
	struct NODERB nodehash[0x10000];
	struct NODE *first;
	struct NODE *last;
	struct HEAP *heap;
	
	/* needed when saving the cache */
	int num_nodes;
	int num_deps;
};

/* */
static unsigned int string_hash(const char *str)
{
	unsigned int h = 0;
	for (; *str; str++)
		h = 31*h + *str;
	return h;
}



/**/
struct GRAPH *node_create_graph(struct HEAP *heap)
{
	/* allocate graph structure */
	struct GRAPH *graph = (struct GRAPH*)mem_allocate(heap, sizeof(struct GRAPH));
	if(!graph)
		return (struct GRAPH *)0x0;

	/* init */
	graph->heap = heap;
	graph->num_nodes = 0;
	graph->first = (struct NODE*)0x0;
	memset(graph->nodehash, 0, sizeof(struct NODE*)*0x10000);
	return graph; 
}

struct HEAP *node_graph_heap(struct GRAPH *graph)
{
	return graph->heap;
}

/* creates a node */
int node_create(struct NODE **nodeptr, struct GRAPH *graph, const char *filename, const char *label, const char *cmdline)
{
	struct NODE *node;
	int sn;

	if(!path_isnice(filename))
		printf("WARNING: adding non nice path %s\n", filename);
	
	/* zero out the return pointer */
	*nodeptr = (struct NODE *)0x0;
	
	/* */
	if(!path_isnice(filename))
		return NODECREATE_NOTNICE;
		
	/* */
	if(node_find(graph, filename))
		return NODECREATE_EXISTS;
	
	/* allocate and set pointers */
	node = (struct NODE *)mem_allocate(graph->heap, sizeof(struct NODE));
	node->graph = graph;
	node->id = graph->num_nodes++;
	node->timestamp = file_timestamp(filename);
	node->firstdep = (struct DEPENDENCY*)0x0;
	node->firstscanner = (struct SCANNER*)0x0;
	
	/* set filename */
	node->filename_len = strlen(filename)+1;
	node->filename = (char *)mem_allocate(graph->heap, node->filename_len);
	memcpy(node->filename, filename, node->filename_len);
	node->hashid = string_hash(filename);

	/* set label line */
	node->label = 0;
	if(label && label[0])
	{
		sn = strlen(label)+1;
		node->label = (char *)mem_allocate(graph->heap, sn);
		memcpy(node->label, label, sn);
	}

	/* set cmdline line */
	node->cmdline = 0;
	if(cmdline && cmdline[0])
	{
		sn = strlen(cmdline)+1;
		node->cmdline = (char *)mem_allocate(graph->heap, sn);
		memcpy(node->cmdline, cmdline, sn);
	}
		
	/* add to hashed tree */
	RB_INSERT(NODERB, &graph->nodehash[node->hashid&0xffff], node);

	/* add to list */
	if(graph->last) graph->last->next = node;
	else graph->first = node;
	node->next = 0;
	graph->last = node;
	
	/* zero out flags */
	node->dirty = 0;
	node->depchecked = 0;
	node->cached = 0;
	node->parenthastool = 0;
	node->counted = 0;
	node->workstatus = NODESTATUS_UNDONE;
	
	/* return new node */
	*nodeptr = node;
	return NODECREATE_OK;
}

/* finds a node based apun the filename */
struct NODE *node_find(struct GRAPH *graph, const char *filename)
{
	unsigned int hashid = string_hash(filename);
	struct NODE tempnode;
	tempnode.hashid = hashid;
	return RB_FIND(NODERB, &graph->nodehash[hashid&0xffff], &tempnode);
}

/* this will return the existing node or create a new one */
struct NODE *node_get(struct GRAPH *graph, const char *filename) 
{
	struct NODE *node = node_find(graph, filename);
	if(!node)
	{
		if(node_create(&node, graph, filename, 0, 0) == NODECREATE_OK)
			return node;
	}
	return node;
}

/* adds a dependency to a node */
struct NODE *node_add_dependency(struct NODE *node, const char *filename)
{
	struct NODE *depnode;
	struct DEPENDENCY *dep;
	
	/* get node (can't fail) */
	depnode = node_get(node->graph, filename);
	
	/* make sure that the node doesn't try to depend on it self */
	if(depnode == node)
	{
		printf("error: this file depends on it self\n  %s\n", node->filename);
		return (struct NODE*)0x0;
	}
	
	/* create dependency */
	dep = (struct DEPENDENCY *)mem_allocate(node->graph->heap, sizeof(struct DEPENDENCY));
	dep->node = depnode;
	
	/* add the dependency to the node */
	dep->next = node->firstdep;
	node->firstdep = dep;
	
	/* set parenttooldep */
	if(node->cmdline)
		depnode->parenthastool = 1;
	
	/* increase dep counter */
	node->graph->num_deps++;
		
	/* return the dependency */
	return depnode;
}

/* functions to handle with bit array access */
static unsigned char *bitarray_allocate(int size)
{ return (unsigned char *)malloc((size+7)/8); }

static void bitarray_zeroall(unsigned char *a, int size)
{ memset(a, 0, (size+7)/8); }

static void bitarray_free(unsigned char *a)
{ free(a); }

static int bitarray_value(unsigned char *a, int id)
{ return a[id>>3]&(1<<(id&0x7)); }

static void bitarray_set(unsigned char *a, int id)
{ a[id>>3] |= (1<<(id&0x7)); }

static void bitarray_clear(unsigned char *a, int id)
{ a[id>>3] &= ~(1<<(id&0x7)); }

/* ************* */
static int node_walk_r(
	struct NODEWALK *walk,
	struct NODE *node)
{
	/* we should detect changes here before we run */
	struct DEPENDENCY *dep;
	struct NODEWALKPATH path;
	int result = 0;
	int needrebuild = 0;
	
	/* check and set mark */
	if(bitarray_value(walk->mark, node->id))
		return 0; 
	bitarray_set(walk->mark, node->id);
	
	if((walk->flags)&NODEWALK_UNDONE)
	{
		if(node->workstatus != NODESTATUS_UNDONE)
			return 0;
	}

	if((walk->flags)&NODEWALK_TOPDOWN)
	{
		walk->node = node;
		result = walk->callback(walk);
	}

	/* push parent */
	path.node = node;
	path.parent = walk->parent;
	walk->parent = &path;
	walk->depth++;
	
	/* build all dependencies */
	for(dep = node->firstdep; dep; dep = dep->next)
	{
		result = node_walk_r(walk, dep->node);
		if(node->timestamp < dep->node->timestamp)
			needrebuild = 1;
		if(result)
			break;
	}

	/* pop parent */
	walk->depth--;
	walk->parent = walk->parent->parent;
	
	/* unmark the node so we can walk this tree again if needed */
	if(!(walk->flags&NODEWALK_QUICK))
		bitarray_clear(walk->mark, node->id);
	
	/* return if we have an error */
	if(result)
		return result;

	/* check if we need to rebuild this node */
	if(!((walk->flags)&NODEWALK_FORCE) && !node->dirty)
		return 0;
	
	/* build */
	if((walk->flags)&NODEWALK_BOTTOMUP)
	{
		walk->node = node;
		result = walk->callback(walk);
	}
	
	return result;
}

int node_walk(
	struct NODE *node,
	int flags,
	int (*callback)(struct NODEWALK*),
	void *u)
{
	struct NODEWALK walk;
	int result;
	
	/* set walk parameters */
	walk.depth = 0;
	walk.flags = flags;
	walk.callback = callback;
	walk.user = u;
	walk.parent = 0;

	/* allocate and clear mark and sweep array */
	walk.mark = bitarray_allocate(node->graph->num_nodes);
	bitarray_zeroall(walk.mark, node->graph->num_nodes);

	/* do the walk */
	result = node_walk_r(&walk, node);

	/* free the array and return */
	bitarray_free(walk.mark);
	return result;
}

/* dumps all nodes to the stdout */
void node_debug_dump(struct GRAPH *graph)
{
	struct NODE *node = graph->first;
	struct DEPENDENCY *dep;
	const char *tool;
	/*const char *workstatus = "UWD";*/
	for(;node;node = node->next)
	{
		static const char d[] = " D";
		tool = "***";
		if(node->cmdline)
			tool = node->cmdline;
		printf("%08x %c   %s   %-15s\n", (unsigned)node->timestamp, d[node->dirty], node->filename, tool);
		for(dep = node->firstdep; dep; dep = dep->next)
			printf("%08x %c      %s\n", (unsigned)dep->node->timestamp, d[dep->node->dirty], dep->node->filename);
	}
}

static int node_debug_dump_tree_r(struct NODEWALK *walkinfo)
{
	/*const char *workstatus = "UWD";*/
	unsigned i;
	if(walkinfo->node->workstatus != NODESTATUS_DONE)
	{
		static const char d[] = " D";
		printf("%3d %08x %c ", walkinfo->depth, (unsigned)walkinfo->node->timestamp, d[walkinfo->node->dirty]);
		for(i = 0; i < walkinfo->depth; i++)
			printf("  ");
		printf("%s   %s\n", walkinfo->node->filename, walkinfo->node->cmdline);
	}
	return 0;
}

void node_debug_dump_tree(struct NODE *root)
{
	node_walk(root, NODEWALK_FORCE|NODEWALK_TOPDOWN, node_debug_dump_tree_r, 0);
}

/* dumps all nodes to the stdout in dot format (graphviz) */
static int node_debug_dump_dot_r(struct NODEWALK *walkinfo)
{
	struct NODE *node = walkinfo->node;
	struct DEPENDENCY *dep;

	if(!node->parenthastool && node != walkinfo->user)
		return 0;
	
	if(node->cmdline)
	{
		for(dep = node->firstdep; dep; dep = dep->next)
			printf("\tn%d -> n%d\n", dep->node->id, node->id);
	}
		
	return 0;
}

void node_debug_dump_dot(struct GRAPH *graph, struct NODE *root_node)
{
	printf("digraph G {\n");
	printf("\tnodesep=.05;");
	printf("\tconcentrate=true;\n");
	printf("\tnode[fontsize=8];\n");

	{
		struct PATH
		{
			struct PATH *next;
			char name[1];
		};

		struct NODE *node;
		struct PATH *first = 0;
		struct PATH *cur;
		char directory[512];
		int found;

		for(node = graph->first; node; node = node->next)
		{
			if(!node->parenthastool && node != root_node)
				continue;
			
			path_directory(node->filename, directory, sizeof(directory));

			found = 0;
			for(cur = first; cur; cur = cur->next)
				if(strcmp(cur->name, directory) == 0)
				{
					found = 1;
					break;
				}

			if(!found)
			{
				cur = (struct PATH*)malloc(sizeof(struct PATH) + strlen(directory));
				cur->next = first;
				strcpy(cur->name, directory);
				first = cur;
			}
		}

		for(cur = first; cur; cur = cur->next)
		{
			printf("subgraph \"cluster_%s\" {\n", cur->name);
			for(node = graph->first; node; node = node->next)
			{
				if(!node->parenthastool && node != root_node)
					continue;
					
				path_directory(node->filename, directory, sizeof(directory));
				if(strcmp(directory, cur->name) == 0)
					printf("\tn%d [label=\"%s\\n%s\"];\n", node->id, path_filename(node->filename), node->cmdline);
			}
			
			printf("}\n");
			
		}
	}

	node_walk(root_node, NODEWALK_FORCE|NODEWALK_BOTTOMUP|NODEWALK_QUICK, node_debug_dump_dot_r, root_node);
	printf("}\n");	
}

static const unsigned bamendianness = 0x01020304;
static char bamheader[8] = {
	'B','A','M',0, /* signature */
	0,3,			/* version */
	sizeof(void*), /* pointer size */
	0, /*((char*)&bamendianness)[0] */ /* endianness */
};

RB_HEAD(CACHENODERB, CACHENODE);

static int cachenode_cmp(struct CACHENODE *a, struct CACHENODE *b)
{
	if(a->hashid > b->hashid) return 1;
	if(a->hashid < b->hashid) return -1;
	return 0;
}

RB_GENERATE_INTERNAL(CACHENODERB, CACHENODE, rbentry, cachenode_cmp, static)

void CACHENODE_FUNCTIONREMOVER() /* this is just to get it not to complain about unused static functions */
{
	(void)CACHENODERB_RB_REMOVE;
	(void)CACHENODERB_RB_NFIND;
	(void)CACHENODERB_RB_NEXT;
	(void)CACHENODERB_RB_PREV;
	(void)CACHENODERB_RB_MINMAX;
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

#define WRITE_BUFFERSIZE (32*1024)
#define WRITE_BUFFERNODES (WRITE_BUFFERSIZE/sizeof(struct CACHENODE))
#define WRITE_BUFFERDEPS (WRITE_BUFFERSIZE/sizeof(unsigned))

#include "platform.h"

/*
	detect if we can use unix styled io. we do this because fwrite
	can use it's own buffers and bam already to it's buffering nicely
	so this will reduce the number of syscalls needed.
*/
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

	#define IO_TYPE int
	#define io_open_read(filename) open(filename, O_RDONLY)
	#define io_open_write(filename) open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666)
	#define io_close(f) close(f)
	#define io_read(f, data, size) read(f, data, size)
	#define io_write(f, data, size) write(f, data, size)
#else
	#define IO_TYPE FILE*
	#define io_open_read(filename) fopen(filename, "rb")
	#define io_open_write(filename) fopen(filename, "wb")
	#define io_close(f) fclose(f)
	#define io_read(f, data, size) fread(data, 1, size, f)
	#define io_write(f, data, size) fwrite(data, 1, size, f)
#endif
	
struct WRITEINFO
{
	IO_TYPE fp;
	
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


static int write_header(struct WRITEINFO *info)
{
	/* setup the cache */
	struct CACHE cache;
	memset(&cache, 0, sizeof(cache));
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
		struct DEPENDENCY *dep;
		
		memset(cachenode, 0, sizeof(cachenode));
		
		cachenode->deps_num = 0;
		for(dep = node->firstdep; dep; dep = dep->next)
			cachenode->deps_num++;
		
		cachenode->hashid = node->hashid;
		cachenode->timestamp = node->timestamp;
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
		struct DEPENDENCY *dep;
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

int node_cache_save(const char *filename, struct GRAPH *graph)
{
	struct WRITEINFO info;
	info.fp = io_open_write(filename);
	if(!info.fp)
		return -1;
	
	info.index = 0;
	info.graph = graph;
	
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

struct CACHE *node_cache_load(const char *filename)
{
	long filesize;
	void *buffer;
	struct CACHE *cache;
	int i;
	size_t itemsread;
	FILE *fp;
	
	/* open file */
	fp = fopen(filename, "rb");
	if(!fp)
		return 0;
		
	/* read the whole file */
	fseek(fp, 0, SEEK_END);
	filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buffer = malloc(filesize);
	
	itemsread = fread(buffer, filesize, 1, fp);
	fclose(fp);
	
	/* verify read and headers */
	cache = (struct CACHE *)buffer;
	
	if(	itemsread != 1 ||
		filesize < sizeof(struct CACHE) ||
		memcmp(cache->header, bamheader, sizeof(bamheader)) != 0 ||
		filesize < sizeof(struct CACHE)+cache->num_nodes*sizeof(struct CACHENODE))
	{
		/* printf("debug: error in headers\n"); */
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

struct CACHENODE *node_cache_find_byindex(struct CACHE *cache, unsigned index)
{
	return &cache->nodes[index];
}

struct CACHENODE *node_cache_find_byhash(struct CACHE *cache, unsigned hashid)
{
	struct CACHENODE tempnode;
	if(!cache)
		return NULL;
	tempnode.hashid = hashid;
	return RB_FIND(CACHENODERB, &cache->nodetree, &tempnode);
}

int node_cache_do_dependency(struct CONTEXT *context, struct NODE *node)
{
	struct CACHENODE *cachenode;
	struct CACHENODE *depcachenode;
	int i;
	
	/* search the cache */
	cachenode = node_cache_find_byhash(context->cache, node->hashid);
	if(cachenode && cachenode->timestamp == node->timestamp)
	{
		/* use cached version */
		for(i = 0; i < cachenode->deps_num; i++)
		{
			depcachenode = node_cache_find_byindex(context->cache, cachenode->deps[i]);
			node_add_dependency(node, depcachenode->filename);
		}
		
		return 1;
	}
	
	return 0;
}
