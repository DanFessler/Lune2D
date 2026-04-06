#include "editor_state.hpp"

#include <atomic>

static std::atomic<std::uint8_t> s_sim_ui_state{
    static_cast<std::uint8_t>(EngSimUiState::Stopped)};
static std::atomic<std::uint32_t> s_selected_entity_id{0};

void eng_editor_set_sim_ui_state(EngSimUiState s)
{
    s_sim_ui_state.store(static_cast<std::uint8_t>(s), std::memory_order_relaxed);
}

EngSimUiState eng_editor_sim_ui_state()
{
    return static_cast<EngSimUiState>(
        s_sim_ui_state.load(std::memory_order_relaxed));
}

void eng_editor_set_selected_entity(std::uint32_t id)
{
    s_selected_entity_id.store(id, std::memory_order_relaxed);
}

std::uint32_t eng_editor_selected_entity()
{
    return s_selected_entity_id.load(std::memory_order_relaxed);
}

bool eng_editor_overlays_enabled()
{
    return eng_editor_sim_ui_state() != EngSimUiState::Playing;
}
