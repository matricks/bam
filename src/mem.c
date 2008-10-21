#include <stdlib.h>
#include <stdio.h>
#include "mem.h"

struct CHUNK
{
	char *memory;
	char *current;
	char *end;
	struct CHUNK *next;
};

struct HEAP
{
	struct CHUNK *current;
};

/* how large each chunk should be */
static const int default_chunksize = 1024*16;

/* allocates a new chunk to be used */
static struct CHUNK *mem_newchunk(int chunksize)
{
	struct CHUNK *chunk;
	char *mem;
	
	/* allocate memory */
	mem = malloc(sizeof(struct CHUNK)+chunksize);
	if(!mem)
		return 0x0;

	/* the chunk structure is located in the begining of the chunk */
	/* init it and return the chunk */
	chunk = (struct CHUNK*)mem;
	chunk->memory = (char*)(chunk+1);
	chunk->current = chunk->memory;
	chunk->end = chunk->memory + chunksize;
	chunk->next = (struct CHUNK *)0x0;
	return chunk;
}

/******************/
static void *mem_allocate_from_chunk(struct CHUNK *chunk, int size)
{
	char *mem;
	
	/* check if we need can fit the allocation */
	if(chunk->current + size > chunk->end)
		return (void*)0x0;

	/* get memory and move the pointer forward */
	mem = chunk->current;
	chunk->current += size;
	return mem;
}

/* creates a heap */
struct HEAP *mem_create()
{
	struct CHUNK *chunk;
	struct HEAP *heap;
	
	/* allocate a chunk and allocate the heap structure on that chunk */
	chunk = mem_newchunk(default_chunksize);
	heap = (struct HEAP *)mem_allocate_from_chunk(chunk, sizeof(struct HEAP));
	heap->current = chunk;
	return heap;
}

/* destroys the heap */
void mem_destroy(struct HEAP *heap)
{
	struct CHUNK *chunk = heap->current;
	struct CHUNK *next;
	
	while(chunk)
	{
		next = chunk->next;
		free(chunk);
		chunk = next;
	}
}

/* */
void *mem_allocate(struct HEAP *heap, int size)
{
	char *mem;

	/* try to allocate from current chunk */
	mem = (char *)mem_allocate_from_chunk(heap->current, size);
	if(!mem)
	{
		if(size > default_chunksize/2)
		{
			/* this block is kinda big, allocate it's own chunk */
			struct CHUNK *chunk = mem_newchunk(size);
			chunk->next = heap->current->next;
			heap->current->next = chunk;
			mem = (char *)mem_allocate_from_chunk(chunk, size);
		}
		else
		{
			/* allocate new chunk and add it to the heap */
			struct CHUNK *chunk = mem_newchunk(default_chunksize);
			chunk->next = heap->current;
			heap->current = chunk;
			
			/* try to allocate again */
			mem = (char *)mem_allocate_from_chunk(heap->current, size);
		}
	}
	
	return mem;
}

void mem_dumpstats(struct HEAP *heap)
{
	struct CHUNK *chunk = heap->current;
	int numchunks = 0;
	int wasted = 0;
	int used = 0;
	int allocated = 0;
	
	while(chunk)
	{
		numchunks++;
		allocated += chunk->end - chunk->memory;
		used += chunk->current - chunk->memory;
		wasted += chunk->end - chunk->current;
		chunk = chunk->next;
	}
	
	printf("chunks: %d\n", numchunks);
	printf("memory allocated: %d\n", allocated	);
	printf("memory used: %d\n", used);
	printf("memory wasted: %d\n", wasted);
}
