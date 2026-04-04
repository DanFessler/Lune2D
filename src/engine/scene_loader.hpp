#pragma once

#include <string>

#include "scene.hpp"

/// Load entities from a JSON scene file into the given scene.
/// Returns true on success.
bool eng_load_scene(Scene& scene, const std::string& jsonPath);

/// Serialize the scene to JSON (explicit entity ids, round-trippable with eng_load_scene).
/// Returns true on success.
bool eng_save_scene(const Scene& scene, const std::string& jsonPath);
