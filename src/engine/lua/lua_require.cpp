#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>

static int l_require(lua_State* L) {
    const char* modname = luaL_checkstring(L, 1);

    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, modname);
    if (!lua_isnil(L, -1))
        return 1;
    lua_pop(L, 1);

    const bool useEnginePath =
        std::strncmp(modname, "editor/", 7) == 0;
    lua_getfield(L, -2, useEnginePath ? "enginepath" : "basepath");
    const char* base = lua_tostring(L, -1);
    if (!base)
        luaL_error(L,
                   useEnginePath ? "package.enginepath not set" : "package.basepath not set");
    std::string path = std::string(base) + modname + ".lua";
    lua_pop(L, 1);

    std::ifstream file(path);
    if (!file.is_open())
        luaL_error(L, "module '%s' not found (tried '%s')", modname, path.c_str());
    std::string source((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());

    size_t bytecodeSize = 0;
    char*  bytecode     = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);
    if (!bytecode)
        luaL_error(L, "require: out of memory compiling '%s'", modname);

    std::string chunkName  = std::string("=") + modname;
    int         loadResult = luau_load(L, chunkName.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);
    if (loadResult != LUA_OK)
        luaL_error(L, "require: load error in '%s': %s", modname, lua_tostring(L, -1));

    if (lua_pcall(L, 0, 1, 0) != LUA_OK)
        luaL_error(L, "require: error in '%s': %s", modname, lua_tostring(L, -1));

    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }

    lua_pushvalue(L, -1);
    lua_setfield(L, -3, modname);
    lua_remove(L, -2);
    lua_remove(L, -2);
    return 1;
}

void eng_lua_register_require(lua_State* L) {
    lua_pushcfunction(L, l_require, "require");
    lua_setglobal(L, "require");
}
