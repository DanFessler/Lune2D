#pragma once

#include <cstdint>

class Scene;

/// World-space pick in Luau coordinates; prefers higher drawOrder, then nearer origin.
/// @return entity id or 0 if none within hit_r.
std::uint32_t eng_editor_pick_entity_at_lu(Scene &scene, float lu_x, float lu_y, float hit_r);
