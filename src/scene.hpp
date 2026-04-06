#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/math.hpp"

struct lua_State;

struct Transform {
    float x = 0, y = 0, angle = 0, vx = 0, vy = 0;
    float sx = 1, sy = 1;
};

struct ScriptInstance {
    std::string           behavior;
    bool                  started = false;
    nlohmann::json        propertyOverrides = nlohmann::json::object();
    /// Luau registry ref for behavior `self` table; LUA_NOREF == -1 when unset.
    /// Valid together with `scriptVmGen` only for that VM generation (`eng_lua_vm_generation`).
    int      luaInstanceRef = -1;
    uint32_t scriptVmGen    = 0;
};

struct Entity {
    uint32_t              id = 0;
    uint32_t              parentId = 0; // 0 = root (no parent)
    std::string           name;
    bool                  active = true;
    int                   drawOrder = 0;
    int                   updateOrder = 0;
    Transform             transform;
    std::vector<ScriptInstance> scripts;
};

class Scene {
public:
    uint32_t spawn(std::string name);
    /// Deserialize: insert entity with a specific id (updates nextId_). Used by scene JSON with "id" fields.
    void insertEntityWithId(uint32_t id, std::string name);
    void     destroy(uint32_t id);
    void     clear();

    Entity*       entity(uint32_t id);
    const Entity* entity(uint32_t id) const;

    bool addScript(uint32_t entityId, const char* behaviorName,
                   const nlohmann::json& propertyOverrides = nlohmann::json::object());
    bool removeScript(uint32_t entityId, int index);
    bool reorderScript(uint32_t entityId, int fromIndex, int toIndex);
    void setDrawOrder(uint32_t entityId, int order);
    void setUpdateOrder(uint32_t entityId, int order);
    void setName(uint32_t entityId, const char* name);
    void setActive(uint32_t entityId, bool active);
    bool setParent(uint32_t entityId, uint32_t parentId);
    void removeParent(uint32_t entityId);
    void setTransformField(uint32_t entityId, const char* field, float value);

    /// After a VM reset, Luau will call `start` again — clear native "already started" flags.
    void resetScriptStartedFlags();

    /// Drops pinned Luau `self` tables for every script (e.g. before behavior hot-reload).
    void releaseAllScriptLuaRefs(lua_State* L);

    /// Clears `luaInstanceRef` on every script without touching Luau (needed after `lua_close` or
    /// when restoring a scene snapshot that still holds registry indices from an old VM).
    void invalidateAllBehaviorLuaRefs();

    /// Stable iteration for tooling: sorted by entity id.
    std::vector<const Entity*> entitiesSortedById() const;

    const std::unordered_map<uint32_t, Entity>& rawEntities() const { return entities_; }

    /// Compute world-space affine matrices for every entity (parent chain composition).
    std::unordered_map<uint32_t, Affine2D> computeWorldMatrices() const;

    /// By drawOrder then id (render pass).
    void forEachEntitySortedByDrawOrder(const std::function<void(Entity&)>& fn);

    /// By updateOrder then id (simulation tick).
    void forEachEntitySortedByUpdateOrder(const std::function<void(Entity&)>& fn);

private:
    std::unordered_map<uint32_t, Entity> entities_;
    uint32_t                              nextId_ = 1;
};
