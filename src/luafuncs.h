
/* support functions */
struct STRINGLIST
{
	struct STRINGLIST *next;
	const char *str;
	size_t len;
};

void build_stringlist(lua_State *L, struct HEAP *heap, struct STRINGLIST **first, int table_index);

/* jobs and deps */
int lf_add_job(struct lua_State *L);
int lf_add_output(struct lua_State *L);
int lf_add_pseudo(struct lua_State *L);
int lf_add_dependency(struct lua_State *L);
int lf_add_constraint_shared(struct lua_State *L);
int lf_add_constraint_exclusive(struct lua_State *L);
int lf_set_filter(struct lua_State *L);
int lf_default_target(struct lua_State *L);
int lf_update_globalstamp(struct lua_State *L);
int lf_nodeexist(struct lua_State *L);

/* dependency */
int lf_add_dependency_cpp_set_paths(lua_State *L); /* dep_cpp.c */
int lf_add_dependency_cpp(lua_State *L); /* dep_cpp.c */
int lf_add_dependency_search(lua_State *L); /* dep_search.c */

/* lua file and directory discovery */
int lf_collect(struct lua_State *L);
int lf_collectrecursive(struct lua_State *L);
int lf_collectdirs(struct lua_State *L);
int lf_collectdirsrecursive(struct lua_State *L);
int lf_listdir(struct lua_State *L);

/* support, files and dirs */
int lf_mkdir(struct lua_State *L);
int lf_mkdirs(struct lua_State *L);
int lf_fileexist(struct lua_State *L);

/* table functions*/
int lf_table_walk(struct lua_State *L);
int lf_table_deepcopy(struct lua_State *L);
int lf_table_tostring(struct lua_State *L);
int lf_table_flatten(struct lua_State *L);

/* support, misc */
int lf_istable(struct lua_State *L);
int lf_isstring(struct lua_State *L);
int lf_loadfile(struct lua_State *L);
int lf_errorfunc(struct lua_State *L);
int lf_panicfunc(struct lua_State *L);

