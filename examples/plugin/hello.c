#include <lua/lua.h>

static int lf_helloworld(struct lua_State *L)
{
	lua_pushstring(L, "Hello World!");
	return 1;
}

int plugin_main(lua_State *L)
{
	lua_register(L, "HelloWorld", lf_helloworld);
	return 0;
}
