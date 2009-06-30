
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
