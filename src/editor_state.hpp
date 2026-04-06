#pragma once

#include <cstdint>

/// Mirrors web Toolbar sim state for Luau editor overlays and tooling.
enum class EngSimUiState : std::uint8_t {
    Stopped = 0,
    Playing = 1,
    Paused = 2,
};

void eng_editor_set_sim_ui_state(EngSimUiState s);
EngSimUiState eng_editor_sim_ui_state();

/// 0 = none (entity ids are always non-zero).
void eng_editor_set_selected_entity(std::uint32_t id);
std::uint32_t eng_editor_selected_entity();

bool eng_editor_overlays_enabled();
