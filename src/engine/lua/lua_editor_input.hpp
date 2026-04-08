#pragma once

#include <cstdint>

struct lua_State;

struct EditorInputState
{
    float mouseX = 0, mouseY = 0;
    float scrollDelta = 0;
    uint32_t buttonsHeld = 0;
    uint32_t buttonsPressed = 0;
    uint32_t buttonsReleased = 0;
};

void eng_editor_input_reset();
void eng_editor_input_set_mouse(float x, float y);
void eng_editor_input_button_event(int button, bool down);
void eng_editor_input_scroll_event(float dy);
/// Call once per frame before `eng_scene_update_editor_behaviors` so `buttonsPressed` /
/// `buttonsReleased` reflect this frame's edges (wheel delta is *not* cleared here).
void eng_editor_input_sync_frame_edges();
/// Call once per frame after editor behavior updates; finalizes `prev_held` and clears wheel delta.
void eng_editor_input_end_frame();
EditorInputState eng_editor_input_state();
bool eng_editor_input_active();

void eng_lua_register_editor_input(lua_State *L);
