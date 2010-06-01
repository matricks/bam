
void install_signals(void (*abortsignal)(int));
int run_command(const char *cmd, const char *filter);

void platform_init();
void platform_shutdown();

/* threading */
void *threads_create(void (*threadfunc)(void *), void *u);
void threads_join(void *thread);
void threads_yield();

void criticalsection_enter();
void criticalsection_leave();

/* filesystem and timestamps */
time_t timestamp();
time_t file_timestamp(const char *filename);
int file_exist(const char *filename);
int file_createdir(const char *path);
void file_touch(const char *filename);

/* plugin related */
typedef int (*PLUGINFUNC)(struct lua_State *);
PLUGINFUNC plugin_load(const char *filename);

/* string hashing function */
unsigned int string_hash(const char *str_in);
unsigned int string_hash_add(unsigned int base, const char *str_in);
