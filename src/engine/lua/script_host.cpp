#include "script_host.hpp"

#include <lua.h>
#include <lualib.h>
#include <luacode.h>

#include <SDL3/SDL.h>

#include <cstdlib>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sys/stat.h>

#include "engine/constants.hpp"
#include "engine/engine.hpp"
#include "lua_api_register.hpp"

std::time_t eng_file_mtime(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;
}

void eng_sync_lua_screen_size(lua_State* L, int w, int h) {
    if (w < 1)
        w = SCREEN_W;
    if (h < 1)
        h = SCREEN_H;
    s_game_lu_w = w;
    s_game_lu_h = h;
    lua_getglobal(L, "screen");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushinteger(L, (lua_Integer)w);
    lua_setfield(L, -2, "w");
    lua_pushinteger(L, (lua_Integer)h);
    lua_setfield(L, -2, "h");
    lua_pop(L, 1);
}

static void eng_load_behaviors(lua_State* L, const std::string& behaviorsDir) {
    DIR* dir = opendir(behaviorsDir.c_str());
    if (!dir) {
        SDL_Log("No behaviors directory: %s", behaviorsDir.c_str());
        return;
    }

    lua_getglobal(L, "_BEHAVIORS");

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        std::string fname = entry->d_name;
        if (fname.size() <= 4 || fname.substr(fname.size() - 4) != ".lua")
            continue;

        std::string name = fname.substr(0, fname.size() - 4);
        std::string modpath = "behaviors/" + name;

        std::string fullpath = behaviorsDir + fname;
        std::ifstream file(fullpath);
        if (!file.is_open()) {
            SDL_Log("Cannot open behavior file: %s", fullpath.c_str());
            continue;
        }
        std::string source((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

        size_t bytecodeSize = 0;
        char*  bytecode = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);
        if (!bytecode) {
            SDL_Log("eng_load_behaviors: compile OOM for %s", name.c_str());
            continue;
        }

        std::string chunkName = std::string("=") + modpath;
        int loadResult = luau_load(L, chunkName.c_str(), bytecode, bytecodeSize, 0);
        free(bytecode);
        if (loadResult != LUA_OK) {
            SDL_Log("eng_load_behaviors: load error '%s': %s", name.c_str(), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
            SDL_Log("eng_load_behaviors: exec error '%s': %s", name.c_str(), lua_tostring(L, -1));
            lua_pop(L, 1);
            continue;
        }

        if (!lua_istable(L, -1)) {
            SDL_Log("eng_load_behaviors: '%s' did not return a table", name.c_str());
            lua_pop(L, 1);
            continue;
        }

        lua_setfield(L, -2, name.c_str());

        // Also cache in package.loaded so require("behaviors/Foo") won't re-run
        lua_getglobal(L, "package");
        lua_getfield(L, -1, "loaded");
        lua_getfield(L, -3, name.c_str()); // get from _BEHAVIORS
        lua_setfield(L, -2, modpath.c_str());
        lua_pop(L, 2); // pop loaded, package

        g_registered_behaviors.push_back(name);
        SDL_Log("Loaded behavior: %s", name.c_str());
    }

    lua_pop(L, 1); // pop _BEHAVIORS
    closedir(dir);
}

namespace {

static const char kUnloadBehaviorModules[] =
    "for k, _ in pairs(package.loaded) do\n"
    "  if type(k) == \"string\" and string.sub(k, 1, 11) == \"behaviors/\" then\n"
    "    package.loaded[k] = nil\n"
    "  end\n"
    "end\n";

} // namespace

void eng_reload_behaviors(lua_State* L, const std::string& luaBaseDir) {
    g_registered_behaviors.clear();

    size_t bytecodeSize = 0;
    char*  bytecode     = luau_compile(kUnloadBehaviorModules,
                                   sizeof(kUnloadBehaviorModules) - 1,
                                   nullptr,
                                   &bytecodeSize);
    if (!bytecode) {
        SDL_Log("eng_reload_behaviors: compile OOM (unload chunk)");
        return;
    }
    int loadResult = luau_load(L, "=eng_reload_behaviors_unload", bytecode, bytecodeSize, 0);
    free(bytecode);
    if (loadResult != LUA_OK) {
        SDL_Log("eng_reload_behaviors: unload load error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        SDL_Log("eng_reload_behaviors: unload run error: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return;
    }

    lua_newtable(L);
    lua_setglobal(L, "_BEHAVIORS");

    std::string baseDir = luaBaseDir;
    if (!baseDir.empty() && baseDir.back() != '/')
        baseDir += '/';
    eng_load_behaviors(L, baseDir + "behaviors/");
}

lua_State* eng_create_lua_vm(const std::string& luaBaseDir) {
    std::string baseDir = luaBaseDir;
    if (!baseDir.empty() && baseDir.back() != '/')
        baseDir += '/';

    g_registered_behaviors.clear();

    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    eng_lua_register_apis(L);

    lua_newtable(L);
    lua_setglobal(L, "_BEHAVIORS");

    lua_newtable(L);
    lua_newtable(L);
    lua_setfield(L, -2, "loaded");
    lua_pushstring(L, baseDir.c_str());
    lua_setfield(L, -2, "basepath");
    lua_setglobal(L, "package");

    eng_load_behaviors(L, baseDir + "behaviors/");

    return L;
}

bool eng_lua_call(lua_State* L, const char* fn, int nargs, int nret) {
    lua_getglobal(L, fn);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        return false;
    }
    if (nargs > 0)
        lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, nret, 0) != LUA_OK) {
        SDL_Log("Lua error in %s: %s", fn, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}
