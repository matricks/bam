
static struct NODETREELINK *nodelinktree_find_closest(struct NODETREELINK *link, unsigned hashid)
{
	if(!link)
		return NULL;
	
	while(1)
	{
		unsigned linkhash = link->node->hashid;
		struct NODETREELINK *leaf = NULL;
		if(linkhash == hashid)
			return link;
		
		leaf = link->leafs[linkhash < hashid];
			
		if(!leaf)
			return link;
		
		link = leaf;
	}
}

static void nodelinktree_rotate_parentswap(struct NODETREELINK *link, struct NODETREELINK *child)
{
	child->parent = link->parent;
	if(link->parent)
	{
		if(link->parent->leafs[0] == link)
			link->parent->leafs[0] = child;
		else
			link->parent->leafs[1] = child;
	}
	link->parent = child;	
}

static void nodelinktree_rotate_depthcalc(struct NODETREELINK *link, struct NODETREELINK *child)
{
	int depth;
	
	link->depth = 1;
	if(link->leafs[0])
		link->depth = link->leafs[0]->depth+1;
	if(link->leafs[1] && link->depth > link->leafs[1]->depth+1)
		link->depth = link->leafs[1]->depth+1;
	
	depth = child->depth;
	for(link = child; link; link = link->parent)
	{
		link->depth = depth;
		depth++;
	}	
}

static struct NODETREELINK *nodelinktree_rotate_right(struct NODETREELINK *link)
{
	struct NODETREELINK *child = link->leafs[0];

	nodelinktree_rotate_parentswap(link, child);
	
	link->leafs[0] = child->leafs[1];
	child->leafs[1] = link;
	if(link->leafs[0])
		link->leafs[0]->parent = link;

	/* redo depth */
	nodelinktree_rotate_depthcalc(link, child);
	return child;
		
}

static struct NODETREELINK *nodelinktree_rotate_left(struct NODETREELINK *link)
{
	struct NODETREELINK *child = link->leafs[1];
	
	nodelinktree_rotate_parentswap(link, child);
	
	link->leafs[1] = child->leafs[0];
	child->leafs[0] = link;
	if(link->leafs[1])
		link->leafs[1]->parent = link;

	/* redo depth */
	nodelinktree_rotate_depthcalc(link, child);
	
	return child;	
}

/* link should be a link at the bottom of the tree */
static struct NODETREELINK *nodelinktree_rebalance(struct NODETREELINK *link)
{
	/* rebalance the tree */
	int direction;
	for(;; link = link->parent)
	{
		direction = 0;
		if(link->leafs[0])
			direction += link->leafs[0]->depth;

		if(link->leafs[1])
			direction -= link->leafs[1]->depth;

		if(direction < -1)
			link = nodelinktree_rotate_left(link);
		else if(direction > 1)
			link = nodelinktree_rotate_right(link);
			
		if(link->parent == NULL)
			return link;
	}
}

static void nodelinktree_insert(struct NODETREELINK **root, struct NODETREELINK *parentlink, struct NODE *node)
{
	struct NODETREELINK *newlink = (struct NODETREELINK *)mem_allocate(node->graph->heap, sizeof(struct NODETREELINK));
	newlink->node = node;
	newlink->depth = 1;

	/* first node special case */
	if(!*root)
	{
		*root = newlink;
		newlink->parent = NULL;
		return;		
	}

	newlink->parent = parentlink;
	
	if(node->hashid > parentlink->node->hashid)
		parentlink->leafs[1] = newlink;
	else
		parentlink->leafs[0] = newlink;

	/* early exit if we didn't make the tree any deeper */
	if(newlink->parent->depth >= 2)
		return;

	/* calculate new depth */
	{
		struct NODETREELINK *link;
		int depth = 2;
		for(link = newlink->parent; link; link = link->parent)
		{
			if(link->depth == depth)
				break;
			link->depth = depth;
			depth++;
		}
	}

	/* rebalance the tree */
	*root = nodelinktree_rebalance(newlink);
}
