#pragma once

class EntityRegistry;

/// Consumes engine entity state and forwards to tooling UIs. Keeps web/UI details out of core types.
void editor_bridge_publish_entity_snapshot(const EntityRegistry& registry);
