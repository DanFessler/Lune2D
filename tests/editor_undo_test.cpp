#include "editor_state.hpp"
#include "editor_undo.hpp"
#include "engine/engine.hpp"
#include "engine/scene_loader.hpp"
#include "scene.hpp"

#include <cassert>
#include <cstring>
#include <string>

static void clear_and_spawn_one()
{
    g_scene.clear();
    g_scene.spawn("Entity");
}

int main()
{
    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    eng_editor_undo_reset();
    clear_and_spawn_one();
    assert(eng_editor_undo_save_state()); // initial milestone

    assert(g_scene.entitiesSortedById().size() == 1);
    uint32_t id = g_scene.entitiesSortedById()[0]->id;

    // One explicit milestone at end of action.
    g_scene.setName(id, "Renamed");
    assert(eng_editor_undo_save_state());
    assert(std::strcmp(g_scene.entity(id)->name.c_str(), "Renamed") == 0);

    assert(eng_editor_undo_try_undo());
    assert(std::strcmp(g_scene.entity(id)->name.c_str(), "Entity") == 0);

    assert(eng_editor_undo_try_redo());
    assert(std::strcmp(g_scene.entity(id)->name.c_str(), "Renamed") == 0);

    // Multi-step edit committed once at end.
    eng_editor_undo_reset();
    clear_and_spawn_one();
    assert(eng_editor_undo_save_state());
    id = g_scene.entitiesSortedById()[0]->id;

    g_scene.setName(id, "B");
    g_scene.setName(id, "C");
    assert(eng_editor_undo_save_state());

    assert(eng_editor_undo_try_undo());
    assert(std::strcmp(g_scene.entity(id)->name.c_str(), "Entity") == 0);

    // No recording while Playing.
    eng_editor_undo_reset();
    clear_and_spawn_one();
    assert(eng_editor_undo_save_state());
    id = g_scene.entitiesSortedById()[0]->id;
    eng_editor_set_sim_ui_state(EngSimUiState::Playing);
    g_scene.setName(id, "PlayEdit");
    assert(eng_editor_undo_save_state() == false);
    assert(eng_editor_undo_can_undo() == false);
    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    eng_editor_undo_reset();

    // Serialize round-trip used by snapshots.
    clear_and_spawn_one();
    id = g_scene.entitiesSortedById()[0]->id;
    g_scene.setName(id, "Round");
    std::string j = eng_scene_to_json_string(g_scene);
    g_scene.clear();
    assert(eng_load_scene_from_json_string(g_scene, j));
    assert(g_scene.entitiesSortedById().size() == 1);
    id = g_scene.entitiesSortedById()[0]->id;
    assert(std::strcmp(g_scene.entity(id)->name.c_str(), "Round") == 0);

    return 0;
}
