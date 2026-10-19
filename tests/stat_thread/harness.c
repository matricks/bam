#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../../src/node.h"

#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_ATOMIC
static int force_empty_load;
static struct NODE *test_atomic_node_load(struct ATOMIC_NODE *ptr)
{
	if(force_empty_load)
	{
		force_empty_load = 0;
		return NULL;
	}
	return atomic_node_load(ptr);
}
#define atomic_node_load test_atomic_node_load
#endif

#define TEST_NODES 1024
static unsigned stat_counts[TEST_NODES];

static time_t test_file_timestamp(const char *filename)
{
	unsigned id;
	if(sscanf(filename, "file%u", &id) != 1 || id >= TEST_NODES)
	{
		fprintf(stderr, "unexpected stat: %s\n", filename);
		exit(1);
	}
	stat_counts[id]++;
	return id % 2 ? 123 : 0;
}

#define file_timestamp test_file_timestamp
#include "../../src/node.c"
#undef file_timestamp

struct SESSION session = {"stat_harness", "stat_harness", 1, 0};

#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_ATOMIC
static int check_completion(void)
{
	struct HEAP *heap = mem_create();
	struct GRAPH *graph = node_graph_create(heap);
	struct NODE *node;
	struct STAT_STATE state;
	state.graph = graph;
	state.node = NULL;
	if(stat_thread_has_work(&state))
		return 5;
	if(node_create(&node, graph, "explicit", NULL, 456) != NODECREATE_OK)
		return 6;
	queue_stat_node(graph, node);
	atomic_flag_set(&graph->statcomplete);
	/* Model an empty link observation just before acquiring completion. */
	force_empty_load = 1;
	if(!stat_thread_has_work(&state) || state.node != node ||
		!stat_thread_has_work(&state) || state.node != NULL)
		return 7;
	mem_destroy(heap);
	return 0;
}
#endif

static int check_graph(unsigned count, int threaded)
{
	struct HEAP *heap = mem_create();
	struct GRAPH *graph = node_graph_create(heap);
	struct NODE *node, *duplicate, *explicit_node;
	unsigned i;
	char name[32];
	memset(stat_counts, 0, sizeof(stat_counts));
	if(threaded)
		node_graph_start_statthread(graph);
	threads_sleep(2);
	for(i = 0; i < count; i++)
	{
		sprintf(name, "file%u", i);
		if(node_create(&node, graph, name, NULL, -1) != NODECREATE_OK ||
			node_create(&duplicate, graph, name, NULL, -1) != NODECREATE_OK ||
			node != duplicate)
			return 1;
		node_cached(node);
		node->skipverifyoutput = 1;
		if(i % 128 == 0)
			threads_sleep(2);
	}
	if(node_create(&explicit_node, graph, "explicit", NULL, 456) != NODECREATE_OK)
		return 2;
	if(threaded)
		node_graph_end_statthread(graph);
	if(explicit_node->timestamp != 456 || explicit_node->timestamp_raw != 456 || explicit_node->dirty)
		return 3;
	for(i = 0; i < count; i++)
	{
		time_t expected = i % 2 ? 123 : 0;
		sprintf(name, "file%u", i);
		node = node_find(graph, name);
		if(stat_counts[i] != 1 || node->timestamp != expected ||
			node->timestamp_raw != expected ||
			node->dirty != (expected ? 0 : NODEDIRTY_MISSING) ||
			!node->cached || !node->skipverifyoutput)
		{
			fprintf(stderr, "node %u: count=%u, timestamp=%ld, dirty=%u\n",
				i, stat_counts[i], (long)node->timestamp, (unsigned)node->dirty);
			return 4;
		}
	}
	mem_destroy(heap);
	return 0;
}

int main(void)
{
	unsigned i;
	int result;
	platform_init();
#if BAM_STAT_THREAD_MODE == BAM_STAT_THREAD_ATOMIC
	result = check_completion();
	if(result)
	{
		fprintf(stderr, "completion recheck failed: %d\n", result);
		return result;
	}
#endif
	for(i = 0; i < 24; i++)
	{
		unsigned count = i < 3 ? i : TEST_NODES;
		result = check_graph(count, 1);
		if(!result)
			result = check_graph(count, 0);
		if(result)
		{
			fprintf(stderr, "stat harness failed: round=%u, code=%d\n", i, result);
			return result;
		}
	}
	platform_shutdown();
	printf("stat harness: " BAM_STAT_THREAD_NAME " passed\n");
	return 0;
}
