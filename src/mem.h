/********************************
 because this application will do lots of smaller
 allocations and never release them, this basic
 memory allocator is optimized for it and will chunk
 memory together so it easily can be released at once.
 when we are done. This also removes allmost all
 overhead per allocation.
**********************************/

struct HEAP *mem_create();
void mem_destroy(struct HEAP *heap);

void *mem_allocate(struct HEAP *heap, int size);
void mem_dumpstats(struct HEAP *heap);

/* TODO: perhaps some sort of string pooling to reduce memory usage? */
