// Tests eng_camera_sync_from_camera_behavior_mutation (inspector-style property sync -> CameraState).

#include "editor_state.hpp"
#include "engine/camera.hpp"
#include "engine/engine.hpp"
#include "engine/lua/behavior_schema.hpp"
#include "scene.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <nlohmann/json.hpp>

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return 1;                                                          \
        }                                                                      \
    } while (0)

lua_State *g_eng_lua_vm = nullptr;

void eng_behavior_release_script_self(lua_State *, ScriptInstance &) {}

static void reset_camera_and_scene()
{
    g_scene.clear();
    eng_behavior_schema_clear();
    eng_behavior_schema_register_builtin_natives();
    eng_behavior_schema_register_test_camera_stub();

    CameraState &c = eng_camera_state();
    c = CameraState{};
    s_game_lu_w = 800;
    s_game_lu_h = 600;
}

static int test_stopped_selected_syncs_editor_vfov_not_play_clear()
{
    reset_camera_and_scene();
    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    eng_editor_set_selected_entity(0);

    uint32_t id = g_scene.spawn("Cam");
    nlohmann::json o = nlohmann::json::object();
    o["vfov"] = 310;
    o["backgroundColor"] = nlohmann::json::array({12, 34, 56, 255});
    TEST_ASSERT(g_scene.addBehavior(id, "Camera", false, o), "add Camera");

    CameraState &cam = eng_camera_state();
    cam.playBgR = 99;
    cam.playBgG = 88;
    cam.playBgB = 77;

    eng_editor_set_selected_entity(id);
    eng_camera_sync_from_camera_behavior_mutation(id, 1);

    TEST_ASSERT(cam.playBgR == 99 && cam.playBgG == 88 && cam.playBgB == 77,
                "play clear RGB unchanged when stopped (editor clear is separate)");
    TEST_ASSERT(std::fabs(cam.playCamVfov - 310.f) < 0.01f, "playCamVfov");
    TEST_ASSERT(std::fabs(cam.editorVfov - 310.f) < 0.01f, "editorVfov preview when stopped+selected");
    return 0;
}

static int test_playing_active_syncs_play_fields_not_editor()
{
    reset_camera_and_scene();
    eng_editor_set_sim_ui_state(EngSimUiState::Playing);

    uint32_t id = g_scene.spawn("Cam");
    nlohmann::json o = nlohmann::json::object();
    o["vfov"] = 288;
    o["backgroundColor"] = nlohmann::json::array({90, 91, 92, 255});
    TEST_ASSERT(g_scene.addBehavior(id, "Camera", false, o), "add Camera");

    CameraState &cam = eng_camera_state();
    cam.activeCameraEntityId = id;
    cam.editorVfov = 111.f; // should not be overwritten by sync when playing

    eng_camera_sync_from_camera_behavior_mutation(id, 1);

    TEST_ASSERT(cam.playBgR == 90 && cam.playBgG == 91 && cam.playBgB == 92, "play bg when playing");
    TEST_ASSERT(std::fabs(cam.playCamVfov - 288.f) < 0.01f, "playCamVfov when playing");
    TEST_ASSERT(std::fabs(cam.editorVfov - 111.f) < 0.01f, "editorVfov untouched when playing");
    return 0;
}

static int test_playing_non_active_does_not_clobber()
{
    reset_camera_and_scene();
    eng_editor_set_sim_ui_state(EngSimUiState::Playing);

    uint32_t a = g_scene.spawn("A");
    uint32_t b = g_scene.spawn("B");
    nlohmann::json ja = nlohmann::json::object();
    ja["vfov"] = 100;
    ja["backgroundColor"] = nlohmann::json::array({200, 0, 0, 255});
    nlohmann::json jb = nlohmann::json::object();
    jb["vfov"] = 400;
    jb["backgroundColor"] = nlohmann::json::array({0, 0, 200, 255});
    TEST_ASSERT(g_scene.addBehavior(a, "Camera", false, ja), "add Camera A");
    TEST_ASSERT(g_scene.addBehavior(b, "Camera", false, jb), "add Camera B");

    CameraState &cam = eng_camera_state();
    cam.activeCameraEntityId = a;
    cam.playCamVfov = 100.f;
    cam.playBgR = 200;
    cam.playBgG = 0;
    cam.playBgB = 0;

    eng_camera_sync_from_camera_behavior_mutation(b, 1);

    TEST_ASSERT(cam.activeCameraEntityId == a, "active unchanged");
    TEST_ASSERT(std::fabs(cam.playCamVfov - 100.f) < 0.01f, "play vfov not overwritten by inactive cam");
    TEST_ASSERT(cam.playBgR == 200 && cam.playBgG == 0 && cam.playBgB == 0, "play bg not overwritten");
    return 0;
}

static int test_vfov_zero_uses_screen_over_ppu()
{
    reset_camera_and_scene();
    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    eng_editor_set_selected_entity(0);

    uint32_t id = g_scene.spawn("Cam");
    nlohmann::json o = nlohmann::json::object();
    o["vfov"] = 0;
    o["backgroundColor"] = nlohmann::json::array({1, 2, 3, 255});
    TEST_ASSERT(g_scene.addBehavior(id, "Camera", false, o), "add Camera");

    eng_camera_state().ppu = 2.f;
    s_game_lu_h = 600;

    eng_editor_set_selected_entity(id);
    eng_camera_sync_from_camera_behavior_mutation(id, 1);

    float expect = 600.f / 2.f;
    TEST_ASSERT(std::fabs(eng_camera_state().playCamVfov - expect) < 0.01f, "auto vfov = screen.h/ppu");
    return 0;
}

int main()
{
    int failures = 0;
#define RUN(fn) do { if (fn()) { failures++; std::fprintf(stderr, "  in " #fn "\n"); } } while (0)
    RUN(test_stopped_selected_syncs_editor_vfov_not_play_clear);
    RUN(test_playing_active_syncs_play_fields_not_editor);
    RUN(test_playing_non_active_does_not_clobber);
    RUN(test_vfov_zero_uses_screen_over_ppu);
#undef RUN
    if (failures == 0)
        std::printf("All camera_sync tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) failed.\n", failures);
    return failures;
}
