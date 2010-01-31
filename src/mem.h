
/*	Because this application will do lots of smaller allocations and
	never release them, this basic memory allocator is optimized for it
	and will chunk memory together so it easily can be released at once
	when we are done. This also removes almost all overhead per
	allocation. The memory is initiated to 0.
	*/

struct HEAP *mem_create();
void mem_destroy(struct HEAP *heap);
void *mem_allocate(struct HEAP *heap, int size);
