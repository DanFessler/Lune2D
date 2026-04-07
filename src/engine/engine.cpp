#include "engine.hpp"

#include <set>

Engine g_eng{};
Scene  g_scene{};
int    s_game_lu_w = SCREEN_W;
int    s_game_lu_h = SCREEN_H;
std::vector<std::string> g_registered_behaviors;

static std::string s_project_lua_dir;

void eng_set_project_lua_dir(std::string path)
{
    while (!path.empty() && (path.back() == '/' || path.back() == '\\'))
        path.pop_back();
    s_project_lua_dir = std::move(path);
}

const std::string& eng_project_lua_dir()
{
    return s_project_lua_dir;
}

std::vector<std::string> eng_editor_behavior_dropdown_names()
{
    std::set<std::string> out;
    for (const auto &b : g_registered_behaviors)
        out.insert(b);
    for (const auto &n : eng_native_attachable_behavior_names())
        out.insert(n);
    return std::vector<std::string>(out.begin(), out.end());
}
