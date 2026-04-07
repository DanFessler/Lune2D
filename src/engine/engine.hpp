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

/// Sorted union of `g_registered_behaviors` and native attachable names (e.g. Transform) for editor UI.
std::vector<std::string> eng_editor_behavior_dropdown_names();

/// Absolute project root (`behaviors/`, `game/`, assets, …). Set once at startup from main.
void                 eng_set_project_lua_dir(std::string path);
const std::string&   eng_project_lua_dir();

inline void eng_init_default_screen_extent() {
    s_game_lu_w = SCREEN_W;
    s_game_lu_h = SCREEN_H;
}
