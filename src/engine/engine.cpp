#include "engine.hpp"

Engine g_eng{};
Scene  g_scene{};
int    s_game_lu_w = SCREEN_W;
int    s_game_lu_h = SCREEN_H;
std::vector<std::string> g_registered_behaviors;
