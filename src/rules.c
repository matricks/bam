#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "context.h"
#include "node.h"
#include "rules.h"
#include "mem.h"

/* */
struct PATTERN *rules_add_pattern(struct GRAPH *graph, const char *patternstr)
{
	hash_t hash = string_hash(patternstr);
	struct PATTERN *pattern = graph->firstpattern;

	/* look if we already got this one */
	for(; pattern; pattern = pattern->next)
		if(pattern->hash == hash)
			return pattern;

	/* create new pattern */
	pattern = (struct PATTERN *)mem_allocate(graph->heap, sizeof(struct PATTERN));
	pattern->string = string_dup(graph->heap, patternstr, strlen(patternstr));
	pattern->hash = hash;
	pattern->next = graph->firstpattern;
	pattern->id = graph->num_patterns;
	graph->firstpattern = pattern;
	graph->num_patterns++;
	return pattern;
}

void rules_finalize_patterns(struct GRAPH *graph)
{
	struct NODE *node;

	/* allocate patternstatus for all nodes */
	for(node = graph->first; node; node = node->next)
		node->patternstatus = mem_allocate(graph->heap, sizeof(unsigned char) * graph->num_patterns);
}

unsigned rules_test_pattern(struct NODE *node, struct PATTERN *pattern)
{
	/* check if we need to do the pattern matching */
	if(node->patternstatus[pattern->id] == PATTENSTATUS_UNKNOWN)
	{
		node->patternstatus[pattern->id] = PATTENSTATUS_NEGATIVE;
		if(strncmp(pattern->string, node->filename, strlen(pattern->string)) == 0)
			node->patternstatus[pattern->id] = PATTENSTATUS_POSITIVE;
	}

	return node->patternstatus[pattern->id] == PATTENSTATUS_POSITIVE;
}

/* this functions takes the whole deferred lookup list and searches for the file */
int rules_test(struct GRAPH *graph, struct DEPENDENCYRULE *rule)
{
	struct PATTERNLINK *patternlink;
	struct NODE *node;
	struct NODELINK *nodelink;
	int numerrors = 0;
	int nodeerrors;

	/* check all nodes against the pattern */
	for(node = graph->first; node; node = node->next)
	{
		if(rules_test_pattern(node, rule->pattern))
		{
			/* check all dependencies against all the patterns */
			nodeerrors = 0;
			for(nodelink = node->firstdep; nodelink; nodelink = nodelink->next)
			{
				for(patternlink = rule->firsttest; patternlink; patternlink = patternlink->next)
				{
					if(rules_test_pattern(nodelink->node, patternlink->pattern))
					{
						numerrors++;
						if(nodeerrors == 0)
						{
							printf("%s: error: violates dependency rules for pattern '%s'\n", node->filename, rule->pattern->string);
							nodeerrors++;
						}

						printf("\t%s violated pattern '%s'\n", nodelink->node->filename, patternlink->pattern->string);
					}
				}
			}
		}
	}

	return numerrors;
}
