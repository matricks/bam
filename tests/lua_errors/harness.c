#define main bam_main
#include "../../src/main.c"
#undef main
#ifdef BAM_FAMILY_WINDOWS
#include <windows.h>
#endif

static int fail_allocation = 0;

static void *test_allocator(void *user, void *ptr, size_t old_size, size_t size)
{
	if(size == 0)
	{
		free(ptr);
		return NULL;
	}
	if(fail_allocation && size > old_size)
		return NULL;
	return realloc(ptr, size);
}

static int failing_handler(lua_State *L)
{
	return luaL_error(L, "handler failure");
}

int main(int argc, char **argv)
{
	lua_State *L = lua_newstate(test_allocator, NULL);
	int status;
	const void *object;
	if(!L)
		return 1;
	if(argc > 2 && strcmp(argv[2], "--win-msvc-mode") == 0)
		session.win_msvcmode = 1;
	if(argc > 1 && strcmp(argv[1], "--panic") == 0)
	{
#ifdef BAM_FAMILY_WINDOWS
		SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif
		lua_atpanic(L, lf_panicfunc);
		lua_pushliteral(L, "panic.lua:7: unprotected failure");
		lua_error(L);
		return 10;
	}
	luaL_openlibs(L);
	session.lua_backtrace = 1;
	session.lua_locals = 1;

	lua_pushcfunction(L, lf_errorfunc);
	if(luaL_loadstring(L, "local detail = 1; error(42)") != LUA_OK)
		return 2;
	status = lua_call_with_report(L, 0);
	if(status != LUA_ERRRUN || lua_gettop(L) != 2 ||
		lua_type(L, -1) != LUA_TNUMBER || lua_tonumber(L, -1) != 42)
		return 3;
	lua_settop(L, 0);

	lua_newtable(L);
	object = lua_topointer(L, -1);
	lua_setglobal(L, "object");
	lua_pushcfunction(L, lf_errorfunc);
	if(luaL_loadstring(L, "error(object)") != LUA_OK)
		return 4;
	status = lua_call_with_report(L, 0);
	if(status != LUA_ERRRUN || lua_gettop(L) != 2 ||
		lua_topointer(L, -1) != object)
		return 5;
	lua_settop(L, 0);
	session.lua_backtrace = 0;
	session.lua_locals = 0;

	lua_pushcfunction(L, lf_errorfunc);
	if(luaL_loadstring(L, "return string.rep('x', 1000000)") != LUA_OK)
		return 6;
	fail_allocation = 1;
	status = lua_call_with_report(L, 0);
	fail_allocation = 0;
	if(status != LUA_ERRMEM || strcmp(lua_tostring(L, -1), "not enough memory") != 0)
		return 7;
	lua_settop(L, 0);

	lua_pushcfunction(L, failing_handler);
	if(luaL_loadstring(L, "error('original')") != LUA_OK)
		return 8;
	status = lua_call_with_report(L, 0);
	if(status != LUA_ERRERR || strcmp(lua_tostring(L, -1), "error in error handling") != 0)
		return 9;
	lua_close(L);
	printf("harness passed\n");
	return 0;
}
