
/* c++ dependency checker */
int dep_cpp(struct CONTEXT *context, struct DEFERRED *info);

/* generic file search checker, used for libs */
struct DEPPLAIN
{
	struct STRINGLIST *firstpath;
	struct STRINGLIST *firstdep;
};

int dep_plain(struct CONTEXT *context, struct DEFERRED *info);
