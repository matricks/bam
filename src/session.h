#include "support.h"

/* session - holds all the settings for this bam session */
struct SESSION
{
	const char *exe;
	const char *name;
	int threads;
	int verbose;
	int simpleoutput;
	
	hash_t cache_hash;

	/* lua options */
	int lua_backtrace;
	int lua_locals;
	
	/* report options */
	int report_color;
	int report_bar;
	int report_steps;

	FILE *eventlog;
	int eventlogflush;

	/* windows options */
	int win_msvcmode;

	volatile int abort; /* raised to 1 when it's time to give up */
};

void install_abort_signal();

/* global session structure */
extern struct SESSION session;
