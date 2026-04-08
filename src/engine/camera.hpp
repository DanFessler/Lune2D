#pragma once

#include "math.hpp"

/// Compute the 2D view matrix from camera parameters (pure function, no Scene/SDL dependency).
/// viewMatrix = translate(screenW/2, screenH/2) * scale(screenH / vfov) * rotate(-angle) * translate(-camX, -camY)
Affine2D eng_compute_view_matrix(float camX, float camY, float camAngle,
                                 float vfov, int screenW, int screenH);

struct CameraState
{
    float ppu = 1.0f;

    // Editor camera (used when not Playing)
    float editorX = 0, editorY = 0, editorAngle = 0, editorVfov = 0;

    // Play camera (clear + projection while Playing / Paused)
    uint32_t activeCameraEntityId = 0;
    float playCamVfov = 0;
    uint8_t playBgR = 0, playBgG = 0, playBgB = 0;

    // Editor / stopped viewport clear (not driven by Camera.backgroundColor)
    uint8_t editorBgR = 0, editorBgG = 0, editorBgB = 0;

    // Computed each frame
    Affine2D viewMatrix;
    Affine2D inverseViewMatrix;
};

CameraState &eng_camera_state();

/// Recompute viewMatrix / inverseViewMatrix from current CameraState + screen dimensions.
/// Uses editor camera when sim state != Playing, otherwise play camera entity.
void eng_camera_compute_view(int screenW, int screenH);

void eng_camera_screen_to_world(float sx, float sy, float *wx, float *wy);
void eng_camera_world_to_screen(float wx, float wy, float *sx, float *sy);

/// Initialize editor camera defaults for given screen size (centers view, identity zoom).
void eng_camera_init_defaults(int screenW, int screenH);

/// After inspector / bridge updates a behavior slot, push Camera props into `CameraState` when
/// the slot is `Camera` and the entity is the active play camera, or selected while stopped (preview).
void eng_camera_sync_from_camera_behavior_mutation(uint32_t entityId, int behaviorIndex);
