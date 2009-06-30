
#define MAX_PATH_LENGTH 512

/*
	session - holds all the settings for this bam session
*/
struct SESSION
{
	const char *name;
	int threads;
	int verbose;
	int simpleoutput;
	int report_color;
	int report_bar;
	int report_steps;
};

extern struct SESSION session;
