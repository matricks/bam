/* Creates a verification state */
struct VERIFY_STATE *verify_create();

/* Destroys a verification state */
void verify_destroy(struct VERIFY_STATE *state);

/*
	Walks trough the current working directory and builds a database of the files.

	A callback is called for every new file (oldstamp is then 0) and
	every known file with the old timestamp and new timestamp. It should return
	the number of errors

	Returns the accumulated number of errors that occured during the update.
*/
int verify_update(struct VERIFY_STATE *state, int (*callback)(const char *fullpath, hash_t hashid, time_t oldstamp, time_t newstamp, void *user), void *user);
