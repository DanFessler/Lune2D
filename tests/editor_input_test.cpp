// TDD tests for Phase 2: Editor input polling API state management.

#include "editor_state.hpp"
#include "engine/lua/lua_editor_input.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static int test_default_mouse_position_zero() {
    eng_editor_input_reset();
    auto s = eng_editor_input_state();
    TEST_ASSERT(s.mouseX == 0.0f, "default mouseX should be 0");
    TEST_ASSERT(s.mouseY == 0.0f, "default mouseY should be 0");
    return 0;
}

static int test_mouse_position_update() {
    eng_editor_input_reset();
    eng_editor_input_set_mouse(100.5f, 200.25f);
    auto s = eng_editor_input_state();
    TEST_ASSERT(std::fabs(s.mouseX - 100.5f) < 0.01f, "mouseX should be 100.5");
    TEST_ASSERT(std::fabs(s.mouseY - 200.25f) < 0.01f, "mouseY should be 200.25");
    return 0;
}

static int test_mouse_button_held() {
    eng_editor_input_reset();
    eng_editor_input_button_event(0, true);
    eng_editor_input_end_frame();
    auto s = eng_editor_input_state();
    TEST_ASSERT(s.buttonsHeld & 1, "left button should be held");
    TEST_ASSERT(!(s.buttonsHeld & 2), "right button should not be held");
    return 0;
}

static int test_mouse_button_pressed_and_released() {
    eng_editor_input_reset();

    // Frame 1: press left button
    eng_editor_input_button_event(0, true);
    eng_editor_input_end_frame();
    auto s1 = eng_editor_input_state();
    TEST_ASSERT(s1.buttonsPressed & 1, "left should be pressed on frame 1");
    TEST_ASSERT(!(s1.buttonsReleased & 1), "left should not be released on frame 1");

    // Frame 2: no new events, button still held
    eng_editor_input_end_frame();
    auto s2 = eng_editor_input_state();
    TEST_ASSERT(s2.buttonsHeld & 1, "left should still be held on frame 2");
    TEST_ASSERT(!(s2.buttonsPressed & 1), "left should not be 'just pressed' on frame 2");

    // Frame 3: release
    eng_editor_input_button_event(0, false);
    eng_editor_input_end_frame();
    auto s3 = eng_editor_input_state();
    TEST_ASSERT(!(s3.buttonsHeld & 1), "left should not be held on frame 3");
    TEST_ASSERT(s3.buttonsReleased & 1, "left should be released on frame 3");

    return 0;
}

static int test_input_inactive_during_play_mode() {
    eng_editor_input_reset();
    eng_editor_set_sim_ui_state(EngSimUiState::Playing);

    eng_editor_input_set_mouse(50.0f, 50.0f);
    eng_editor_input_button_event(0, true);
    eng_editor_input_end_frame();

    TEST_ASSERT(!eng_editor_input_active(), "input should be inactive during play");

    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    TEST_ASSERT(eng_editor_input_active(), "input should be active when stopped");

    eng_editor_set_sim_ui_state(EngSimUiState::Paused);
    TEST_ASSERT(eng_editor_input_active(), "input should be active when paused");
    return 0;
}

int main() {
    int failures = 0;

#define RUN(fn) do { if (fn()) { failures++; std::fprintf(stderr, "  in " #fn "\n"); } } while(0)

    RUN(test_default_mouse_position_zero);
    RUN(test_mouse_position_update);
    RUN(test_mouse_button_held);
    RUN(test_mouse_button_pressed_and_released);
    RUN(test_input_inactive_during_play_mode);

#undef RUN

    if (failures == 0)
        std::printf("All editor input tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) failed.\n", failures);
    return failures;
}
