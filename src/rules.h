
/* dependency rule */
struct DEPENDENCYRULE
{
	struct PATTERN *pattern;
	struct PATTERNLINK *firsttest;
};

/* */
struct PATTERN
{
	struct PATTERN *next;
	struct NODELINK *firstnode;
	const char *string;
	hash_t hash;
	unsigned id;
};

/* */
struct PATTERNLINK
{
	struct PATTERNLINK *next;
	struct PATTERN *pattern;
};

/* adds a pattern to the graph*/
struct PATTERN *rules_add_pattern(struct GRAPH *graph, const char *pattern);

/* freezes the patterns and does the setup to start matching */
void rules_finalize_patterns(struct GRAPH *graph);

/* returns non-zero if the pattern matches the node */
unsigned rules_test_pattern(struct NODE *node, struct PATTERN *pattern);

/* */
int rules_test(struct GRAPH *graph, struct DEPENDENCYRULE *rule);
