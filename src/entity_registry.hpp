#pragma once

#include <string>
#include <vector>

/// Runtime entity records (transform + labels). Owned by the engine; scripts sync via bindings.
struct EntityRecord {
    std::string id;
    std::string name;
    float       x = 0, y = 0, angle = 0, vx = 0, vy = 0;
};

/// Frame-coherent list: cleared each tick at the host boundary; gameplay adds current entities.
class EntityRegistry {
public:
    void clear() { m_records.clear(); }

    void add(std::string id, std::string name,
             float x, float y, float angle, float vx, float vy) {
        m_records.push_back({std::move(id), std::move(name), x, y, angle, vx, vy});
    }

    const std::vector<EntityRecord>& records() const { return m_records; }

private:
    std::vector<EntityRecord> m_records;
};
