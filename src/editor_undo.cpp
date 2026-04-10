#include "editor_undo.hpp"

#include "editor_state.hpp"
#include "engine/engine.hpp"
#include "engine/lua/lua_vm_lifecycle.hpp"
#include "engine/scene_loader.hpp"
#include "scene.hpp"

#include <SDL3/SDL.h>
#include <deque>

namespace {

constexpr std::size_t kMaxUndo = 64;

bool                    s_restoring = false;
std::deque<std::string> s_states;
std::size_t             s_cursor = 0;

bool can_record_now()
{
    if (s_restoring)
        return false;
    if (eng_editor_sim_ui_state() == EngSimUiState::Playing)
        return false;
    return true;
}

bool apply_snapshot_string(const std::string& json)
{
    if (!eng_load_scene_from_json_string(g_scene, json))
    {
        SDL_Log("editor_undo: failed to apply snapshot");
        return false;
    }
    eng_on_lua_vm_replaced(g_scene);
    g_scene.resetScriptStartedFlags();
    return true;
}

} // namespace

void eng_editor_undo_reset()
{
    s_states.clear();
    s_cursor = 0;
}

bool eng_editor_undo_save_state()
{
    if (!can_record_now())
        return false;

    const std::string snap = eng_scene_to_json_string(g_scene);
    if (!s_states.empty())
    {
        if (s_cursor >= s_states.size())
            s_cursor = s_states.size() - 1;

        if (s_states[s_cursor] == snap)
            return false;

        // New branch after undo: drop redo tail.
        s_states.erase(s_states.begin() + static_cast<long long>(s_cursor) + 1, s_states.end());
    }

    s_states.push_back(snap);

    if (s_states.size() > kMaxUndo)
        s_states.pop_front();

    s_cursor = s_states.empty() ? 0 : (s_states.size() - 1);
    return true;
}

bool eng_editor_undo_try_undo()
{
    if (eng_editor_sim_ui_state() == EngSimUiState::Playing)
        return false;
    if (s_states.size() < 2)
        return false;
    if (s_cursor == 0)
        return false;

    s_restoring = true;
    const std::size_t target = s_cursor - 1;
    const bool ok = apply_snapshot_string(s_states[target]);
    s_restoring = false;
    if (!ok)
    {
        eng_editor_undo_reset();
        return false;
    }

    s_cursor = target;
    return true;
}

bool eng_editor_undo_try_redo()
{
    if (eng_editor_sim_ui_state() == EngSimUiState::Playing)
        return false;
    if (s_states.size() < 2)
        return false;
    if (s_cursor + 1 >= s_states.size())
        return false;

    s_restoring = true;
    const std::size_t target = s_cursor + 1;
    const bool ok = apply_snapshot_string(s_states[target]);
    s_restoring = false;
    if (!ok)
    {
        eng_editor_undo_reset();
        return false;
    }

    s_cursor = target;
    return true;
}

bool eng_editor_undo_can_undo()
{
    if (eng_editor_sim_ui_state() == EngSimUiState::Playing)
        return false;
    return s_states.size() >= 2 && s_cursor > 0;
}

bool eng_editor_undo_can_redo()
{
    if (eng_editor_sim_ui_state() == EngSimUiState::Playing)
        return false;
    return s_states.size() >= 2 && (s_cursor + 1 < s_states.size());
}
