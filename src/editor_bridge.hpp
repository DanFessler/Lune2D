#pragma once

class Scene;

/// Publishes scene + components to tooling UIs as JSON.
void editor_bridge_publish_scene_snapshot(const Scene& scene);
