#include "camera.hpp"

#include "editor_state.hpp"
#include "engine/engine.hpp"
#include "engine/lua/behavior_schema.hpp"
#include "scene.hpp"

#include <algorithm>
#include <cmath>

static CameraState s_camera;

CameraState &eng_camera_state()
{
    return s_camera;
}

void eng_camera_init_defaults(int screenW, int screenH)
{
    float ppu = s_camera.ppu;
    s_camera.editorX = screenW / (2.f * ppu);
    s_camera.editorY = screenH / (2.f * ppu);
    s_camera.editorAngle = 0;
    s_camera.editorVfov = screenH / ppu;
    s_camera.viewMatrix = Affine2D::identity();
    s_camera.inverseViewMatrix = Affine2D::identity();
}

void eng_camera_compute_view(int screenW, int screenH)
{
    if (screenW < 1)
        screenW = 1;
    if (screenH < 1)
        screenH = 1;

    static int s_last_w = 0;
    static int s_last_h = 0;

    if (s_camera.editorVfov < 1e-6f)
        eng_camera_init_defaults(screenW, screenH);
    else if (s_last_w != screenW || s_last_h != screenH)
    {
        float expected_vfov = (s_last_h > 0) ? (float)s_last_h / s_camera.ppu : 0.f;
        if (s_last_w == 0 || std::fabs(s_camera.editorVfov - expected_vfov) < 0.01f)
            eng_camera_init_defaults(screenW, screenH);
    }

    s_last_w = screenW;
    s_last_h = screenH;

    float camX = s_camera.editorX;
    float camY = s_camera.editorY;
    float camAngle = s_camera.editorAngle;
    float vfov = s_camera.editorVfov;

    s_camera.viewMatrix = eng_compute_view_matrix(camX, camY, camAngle, vfov, screenW, screenH);
    s_camera.inverseViewMatrix = s_camera.viewMatrix.inverse();
}

void eng_camera_screen_to_world(float sx, float sy, float *wx, float *wy)
{
    Vec2 w = s_camera.inverseViewMatrix.transformPoint(sx, sy);
    if (wx) *wx = w.x;
    if (wy) *wy = w.y;
}

void eng_camera_world_to_screen(float wx, float wy, float *sx, float *sy)
{
    Vec2 s = s_camera.viewMatrix.transformPoint(wx, wy);
    if (sx) *sx = s.x;
    if (sy) *sy = s.y;
}

void eng_camera_sync_from_camera_behavior_mutation(uint32_t entityId, int behaviorIndex)
{
    Entity *e = g_scene.entity(entityId);
    if (!e || behaviorIndex < 0 || behaviorIndex >= (int)e->behaviors.size())
        return;
    BehaviorSlot &slot = e->behaviors[behaviorIndex];
    if (slot.isNative || slot.script.behavior != "Camera")
        return;

    nlohmann::json merged =
        eng_behavior_merge_properties("Camera", slot.script.propertyOverrides);

    float ppu = s_camera.ppu;
    float vfov = 0.f;
    if (merged.contains("vfov") && merged["vfov"].is_number())
        vfov = (float)merged["vfov"].get<double>();
    if (vfov <= 0.f)
        vfov = (float)s_game_lu_h / ppu;

    uint8_t r = 0, g = 0, b = 0;
    if (merged.contains("backgroundColor") && merged["backgroundColor"].is_array())
    {
        const auto &arr = merged["backgroundColor"];
        if (arr.size() >= 3)
        {
            r = (uint8_t)std::clamp((int)std::lround(arr[0].get<double>()), 0, 255);
            g = (uint8_t)std::clamp((int)std::lround(arr[1].get<double>()), 0, 255);
            b = (uint8_t)std::clamp((int)std::lround(arr[2].get<double>()), 0, 255);
        }
    }

    const EngSimUiState sim = eng_editor_sim_ui_state();
    const bool stopped = (sim == EngSimUiState::Stopped);
    const bool activeMatch = (s_camera.activeCameraEntityId == entityId);
    const bool simUsesPlayCam =
        (sim == EngSimUiState::Playing || sim == EngSimUiState::Paused);

    if (activeMatch && simUsesPlayCam)
    {
        s_camera.playCamVfov = vfov;
        s_camera.playBgR = r;
        s_camera.playBgG = g;
        s_camera.playBgB = b;
    }

    if (stopped && eng_editor_selected_entity() == entityId)
    {
        s_camera.playCamVfov = vfov;
        s_camera.editorVfov = vfov;
        // Background color applies to the play camera only; do not tint the editor clear.
    }
}
