#include <lua.h>
#include <lualib.h>

#include <cstring>

#include "engine/engine.hpp"

static int l_audio_play(lua_State* L) {
    const char*  name = luaL_checkstring(L, 1);
    AudioSystem& a    = g_eng.audio;
    if (!strcmp(name, "shoot"))
        a.play(a.sndShoot);
    else if (!strcmp(name, "exp_large"))
        a.play(a.sndExpLarge);
    else if (!strcmp(name, "exp_medium"))
        a.play(a.sndExpMedium);
    else if (!strcmp(name, "exp_small"))
        a.play(a.sndExpSmall);
    else if (!strcmp(name, "death"))
        a.play(a.sndDeath);
    else if (!strcmp(name, "beat0"))
        a.play(a.sndBeat[0]);
    else if (!strcmp(name, "beat1"))
        a.play(a.sndBeat[1]);
    return 0;
}

static int l_audio_thrust(lua_State* L) {
    bool on = lua_toboolean(L, 1);
    g_eng.audio.updateThrust(on);
    return 0;
}

void eng_lua_register_audio(lua_State* L) {
    lua_newtable(L);
    lua_pushcfunction(L, l_audio_play, "audio.play");
    lua_setfield(L, -2, "play");
    lua_pushcfunction(L, l_audio_thrust, "audio.thrust");
    lua_setfield(L, -2, "thrust");
    lua_setglobal(L, "audio");
}
