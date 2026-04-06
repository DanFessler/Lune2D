#include "engine.hpp"

#include <set>

Engine g_eng{};
Scene  g_scene{};
int    s_game_lu_w = SCREEN_W;
int    s_game_lu_h = SCREEN_H;
std::vector<std::string> g_registered_behaviors;

std::vector<std::string> eng_editor_behavior_dropdown_names()
{
    std::set<std::string> out;
    for (const auto &b : g_registered_behaviors)
        out.insert(b);
    for (const auto &n : eng_native_attachable_behavior_names())
        out.insert(n);
    return std::vector<std::string>(out.begin(), out.end());
}
