
/* jobs and deps */
int lf_add_job(struct lua_State *L);
int lf_add_output(struct lua_State *L);
int lf_add_dependency(struct lua_State *L);
int lf_add_dependency_search(struct lua_State *L);
int lf_add_constraint_shared(struct lua_State *L);
int lf_add_constraint_exclusive(struct lua_State *L);
int lf_set_touch(struct lua_State *L);
int lf_set_filter(struct lua_State *L);
int lf_default_target(struct lua_State *L);
int lf_update_globalstamp(struct lua_State *L);

/* lua file and directory discovery */
int lf_collect(struct lua_State *L);
int lf_collectrecursive(struct lua_State *L);
int lf_collectdirs(struct lua_State *L);
int lf_collectdirsrecursive(struct lua_State *L);
int lf_listdir(struct lua_State *L);

/* support, files and dirs */
int lf_mkdir(struct lua_State *L);
int lf_fileexist(struct lua_State *L);

/* support, misc */
int lf_istable(struct lua_State *L);
int lf_isstring(struct lua_State *L);
int lf_loadfile(struct lua_State *L);
int lf_errorfunc(struct lua_State *L);
int lf_panicfunc(struct lua_State *L);

