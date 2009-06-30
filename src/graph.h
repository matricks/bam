#include "node.h"
#include "tree.h"

RB_HEAD(NODERB, NODE);

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
