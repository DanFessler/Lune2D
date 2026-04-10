#pragma once

#include <string>

#include "scene.hpp"

/// Load entities from a JSON scene file into the given scene.
/// Returns true on success.
bool eng_load_scene(Scene& scene, const std::string& jsonPath);

/// Replace `scene` from JSON text (same format as scene files). Returns true on success.
bool eng_load_scene_from_json_string(Scene& scene, const std::string& jsonUtf8);

/// Serialize the scene to compact JSON (explicit entity ids; round-trippable with loads).
std::string eng_scene_to_json_string(const Scene& scene);

/// Serialize the scene to JSON (explicit entity ids, round-trippable with eng_load_scene).
/// Returns true on success.
bool eng_save_scene(const Scene& scene, const std::string& jsonPath);
