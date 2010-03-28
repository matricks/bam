/* lua includes */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "platform.h"

#define PATH_SEPARATOR '/'

static unsigned path_is_separator(char c)
{
#if defined(BAM_FAMILY_WINDOWS)
	return c == '/' || c == '\\';
#else
	return c == '/';
#endif
}

/* */
const char *path_filename(const char *path)
{
	const char *ret = path;
	const char *cur;
	for(cur = path; *cur; cur++)
	{
		if(path_is_separator(*cur))
			ret = cur+1;
	}
	return ret;
}

/* */
int path_directory(const char *path, char *directory, int size)
{
	char *dest = directory;
	char *dest_end = directory+size-1;
	const char *read = path;
	const char *cur;

	for(cur = path; *cur; cur++)
	{
		if(path_is_separator(*cur))
		{
			/* ok, copy the directory */
			for(; read != cur; read++, dest++)
			{
				if(dest == dest_end)
				{
					*dest =  0;
					return 1;
				}
				*dest = *read;
			}
		}
	}
	
	*dest = 0;
	if(0)
		printf("path_directory:\n\tinput:'%s'\n\toutput:'%s'\n", path, directory);
	return 0;
}

/* normalizes the path, it rewrites the path */
int path_normalize(char *path)
{
	char *dirs[128];
	int depth = 0;
	char *dstptr = path;
	char *srcptr = path;
	
	/* add the start */
	dirs[0] = path;
	depth++;
	
	while(1)
	{
		if(srcptr[0] == '.')
		{
			if(path_is_separator(srcptr[1]))
			{
				/* "./" case, just skip the data */
				srcptr += 2;
			}
			else if(srcptr[1] == '.')
			{
				/* found ".." in path */
				if(path_is_separator(srcptr[2]))
				{
					/* "../" case */
					if(depth == 1)
					{
						/* case where we are at the start so append ../ to the start of the string */
						dstptr[0] = '.';
						dstptr[1] = '.';
						dstptr[2] = '/';
						dstptr += 3;
						srcptr += 3;
						
						dirs[0] = dstptr;
					}
					else
					{
						/* normal case where we are in the middle like "a/b/../c" */
						depth--;
						dstptr = dirs[depth-1];
						srcptr += 3;
					}
				}
				else if(srcptr[2] == 0)
				{
					/* ".." case, .. at end of string */
					if(depth == 1)
					{
						dstptr[0] = '.';
						dstptr[1] = '.';
						dstptr += 2;
						srcptr += 2;
						
						dirs[0] = dstptr;
					}
					else
					{
						depth--;
						dstptr = dirs[depth-1];
						srcptr += 2;
					}
				}
				else
				{
					/* "..?" case */
					return -1;
				}
			}
		}
		else
		{
			/* search for separator */
			while(!path_is_separator(srcptr[0]) && srcptr[0])
				*dstptr++ = *srcptr++;
			
			if(srcptr[0] == 0)
			{
				/* end of string, zero terminate and return, strip ending '/' if it exists */
				if(dstptr != path && dstptr[-1] == '/')
					dstptr[-1] = 0;
				dstptr[0] = 0;
				return 0;
			}
			else if(path_is_separator(srcptr[0]))
			{
				/* store the point of this directory */
				*dstptr++ = *srcptr++;
				dirs[depth] = dstptr;
				depth++;
			}
			else
			{
				/* non reachable case */
				return -1;
			}
		}
	}
	
	return 0;
}

/* returns true if a path is absolute */
int path_isabs(const char *path)
{
#if defined(BAM_FAMILY_WINDOWS)
	if(strlen(path) > 2 && isalpha(path[0]) && path[1] == ':' && path_is_separator(path[2]))
		return 1;
#elif defined(BAM_FAMILY_UNIX)
	if(path_is_separator(path[0]))
		return 1;
#elif defined(BAM_FAMILY_BEOS)
	if(path_is_separator(path[0]))
		return 1;
#else
	#error path_isabs(const char *path) not implemented on this platform
#endif
	
	return 0;
}

/* is it absolute and normalized? */
int path_isnice(const char *path)
{
	/* check for initial "../../" */
	while(path[0] == '.')
	{
		if(path[1] == '.')
		{
			if(path_is_separator(path[2]))
			{
				/* found "../" case */
				path += 3;
			}
			else
				return 0;
		}
		else if(path_is_separator(path[1]))
			return 0;
		else
			break;
	}
	
	while(path[0])
	{
		if(path_is_separator(path[0]))
		{
			/* check for // */
			if(path_is_separator(path[1]))
				return 0;
			
			if(path[1] == '.')
			{
				/* check for /.. */
				if(path[2] == '.')
					return 0;
				
				/* check for /./ */
				if(path_is_separator(path[2]))
					return 0;
			}
			
			/* check so that the path doesn't end on / */
			if(path[1] == 0)
				return 0;
		}
		path++;
	}
	
	return 1;
}

/* return zero on success */
int path_join(const char *base, const char *extend, char *output, int size)
{
	int i;
	int elen = strlen(extend);
	int blen;
	
	if(path_isabs(extend))
	{
		/* just copy the extend path */
		if(elen+1 > size)
			return 1;
		
		for(i = 0; i < elen+1; i++)
			output[i] = extend[i];

		path_normalize(output);
		return 0;
	}
	
	blen = strlen(base);
	
	if(blen+elen+2 > size)
		return 1;

	if(!path_isabs(base))
		return 1;
	
	/* copy base path */
	for(i = 0; i < blen; i++)
		output[i] = base[i];
	
	/* add separator */
	output[blen] = PATH_SEPARATOR;
	
	/* append the extra path, and null-terminator*/
	for(i = 0; i < elen+1; i++)
		output[blen+1+i] = extend[i];
	
	/* normalize path and return success */
	path_normalize(output);
	return 0;
}

/*  */
int lf_path_join(lua_State *L)
{
	char buffer[1024*2];
	int n = lua_gettop(L);
	if(n < 2)
		luaL_error(L, "path_join: incorrect number of arguments");
	
	if(!path_join(lua_tostring(L, 1), lua_tostring(L, 2), buffer, 2*1024))
	{
		luaL_error(L, "path_join: couldn't join\n\t%s\n  and\n\t%s",
			lua_tostring(L, 1),
			lua_tostring(L, 2));
	}
	
	lua_pushstring(L, buffer);
	return 1;
}

/*  */
int lf_path_isnice(lua_State *L)
{
	int n = lua_gettop(L);
	const char *path = 0;
	if(n < 1)
		luaL_error(L, "path_isnice: incorrect number of arguments");
	
	path = lua_tostring(L, 1);
	lua_pushnumber(L, path_isnice(path));
	return 1;
}

int lf_path_normalize(lua_State *L)
{
	int n = lua_gettop(L);
	const char *path = 0;

	if(n < 1)
		luaL_error(L, "path_normalize: incorrect number of arguments");
	
	path = lua_tostring(L, 1);
	if(!path)
		luaL_error(L, "path_normalize: got null-string");
	
	if(path_isnice(path))
	{
		/* path is ok */
		lua_pushstring(L, path);
	}
	else
	{
		/* normalize and return */
		char buffer[2*1024];
		strcpy(buffer, path);
		path_normalize(buffer);
		lua_pushstring(L, buffer);
	}
	
	return 1;
}


static const char *path_ext(const char *filename)
{
	const char *cur = filename;
	const char *ext = 0;
	
	for(; *cur; cur++)
	{
		if(*cur == '.')
			ext = cur;
		if(*cur == '/')
			ext = (const char *)0x0;
	}
	if(!ext)
		return "";
	return ext+1;
}

/*  */
int lf_path_ext(lua_State *L)
{
	int n = lua_gettop(L);
	const char *path = 0;
	if(n < 1)
		luaL_error(L, "path_ext: incorrect number of arguments");
	
	path = lua_tostring(L, 1);
	if(!path)
		luaL_error(L, "path_ext: argument is not a string");
		
	lua_pushstring(L, path_ext(path));
	return 1;
}


/*  */
int lf_path_base(lua_State *L)
{
	int n = lua_gettop(L);
	size_t org_len;
	size_t new_len;
	size_t count = 0;
	const char *cur = 0;
	const char *path = 0;
	if(n < 1)
		luaL_error(L, "path_base: incorrect number of arguments");
	
	path = lua_tolstring(L, 1, &org_len);
	if(!path)
		luaL_error(L, "path_base: argument is not a string");

	/* cut off the ext */
	new_len = org_len;
	for(cur = path; *cur; cur++, count++)
	{
		if(*cur == '.')
			new_len = count;
		else if(*cur == '/')
			new_len = org_len;
	}
		
	lua_pushlstring(L, path, new_len);
	return 1;
}


static int path_path_length(const char *path)
{
	const char *cur = path;
	int total = 0;
	int len = -1;
	for(; *cur; cur++, total++)
	{
		if(*cur == '/')
			len = (int)(cur-path);
	}

	if(len == -1)
		return 0;
	return len;
}

/*  */
int lf_path_path(lua_State *L)
{
	char buffer[1024];
	int n = lua_gettop(L);
	const char *path = 0;
	if(n < 1)
		luaL_error(L, "path_path: incorrect number of arguments");

	path = lua_tostring(L, 1);
	if(!path)
		luaL_error(L, "path_path: argument is not a string");
	
	/* check if we can take the easy way out */
	if(path_isnice(path))
	{
		lua_pushlstring(L, path, path_path_length(path));
		return 1;
	}
	
	/* we must normalize the path as well */
	strncpy(buffer, path, sizeof(buffer));
	path_normalize(buffer);
	lua_pushlstring(L, buffer, path_path_length(buffer));
	return 1;
}

/*  */
int lf_path_filename(lua_State *L)
{
	int n = lua_gettop(L);
	const char *path = 0;
	if(n < 1)
		luaL_error(L, "path_filename: incorrect number of arguments");

	path = lua_tostring(L, 1);

	if(!path)
		luaL_error(L, "path_filename: null name");

	lua_pushstring(L, path_filename(path));
	return 1;
}
