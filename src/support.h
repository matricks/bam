
void install_signals(void (*abortsignal)(int));

void platform_init();
void platform_shutdown();

void *threads_create(void (*threadfunc)(void *), void *u);
void threads_join(void *thread);
void threads_yield();

void criticalsection_enter();
void criticalsection_leave();

time_t timestamp();
time_t file_timestamp(const char *filename);
int file_exist(const char *filename);

int lf_collect(struct lua_State *L);
int lf_collectrecursive(struct lua_State *L);
int lf_collectdirs(lua_State *L);
int lf_collectdirsrecursive(lua_State *L);
int lf_listdir(struct lua_State *L);
