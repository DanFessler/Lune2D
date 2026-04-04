#pragma once

#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "audio_system.hpp"
#include "scene.hpp"

#include "constants.hpp"

/// Host application + rendering + input snapshot (cf. ctx `Game` + window).
struct Engine {
    SDL_Window*     window   = nullptr;
    SDL_Renderer*   renderer = nullptr;
    AudioSystem     audio;
    const bool*     keys     = nullptr;
    bool            quit     = false;
};

extern Engine g_eng;
extern Scene  g_scene;
/// Luau / draw.clear extent (matches #game-surface when inset; else full window logical).
extern int s_game_lu_w;
extern int s_game_lu_h;
/// Behavior names registered via runtime.registerBehavior (populated by Lua, read by bridge).
extern std::vector<std::string> g_registered_behaviors;

inline void eng_init_default_screen_extent() {
    s_game_lu_w = SCREEN_W;
    s_game_lu_h = SCREEN_H;
}
