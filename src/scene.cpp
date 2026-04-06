#include "scene.hpp"

#include "engine/lua/behavior_instance.hpp"
#include "engine/lua/lua_runtime.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

const char *kNativeAttachableBehaviors[] = {"Transform", nullptr};

} // namespace

bool eng_behavior_name_is_native_attachable(const char *name)
{
    if (!name || name[0] == '\0')
        return false;
    for (int i = 0; kNativeAttachableBehaviors[i]; ++i)
        if (std::strcmp(name, kNativeAttachableBehaviors[i]) == 0)
            return true;
    return false;
}

std::vector<std::string> eng_native_attachable_behavior_names()
{
    std::vector<std::string> out;
    for (int i = 0; kNativeAttachableBehaviors[i]; ++i)
        out.emplace_back(kNativeAttachableBehaviors[i]);
    return out;
}

// ── Entity convenience accessors ──

Transform *Entity::getTransform()
{
    for (auto &b : behaviors)
        if (b.isNative && b.name == "Transform")
            return &b.transform;
    return nullptr;
}

const Transform *Entity::getTransform() const
{
    for (const auto &b : behaviors)
        if (b.isNative && b.name == "Transform")
            return &b.transform;
    return nullptr;
}

int Entity::getTransformIndex() const
{
    for (int i = 0; i < (int)behaviors.size(); ++i)
        if (behaviors[i].isNative && behaviors[i].name == "Transform")
            return i;
    return -1;
}

// ── Scene lifecycle ──

uint32_t Scene::spawn(std::string name)
{
    uint32_t id = nextId_++;
    Entity e;
    e.id = id;
    e.name = name.empty() ? "Entity" : std::move(name);
    entities_.emplace(id, std::move(e));
    addBehavior(id, "Transform", true);
    return id;
}

void Scene::insertEntityWithId(uint32_t id, std::string name)
{
    Entity e;
    e.id = id;
    e.name = name.empty() ? "Entity" : std::move(name);
    entities_[id] = std::move(e);
    if (id >= nextId_)
        nextId_ = id + 1;
}

void Scene::destroy(uint32_t id)
{
    auto it = entities_.find(id);
    if (it == entities_.end())
        return;
    if (g_eng_lua_vm)
    {
        for (auto &b : it->second.behaviors)
            if (!b.isNative)
                eng_behavior_release_script_self(g_eng_lua_vm, b.script);
    }
    entities_.erase(it);
}

void Scene::clear()
{
    if (g_eng_lua_vm)
    {
        for (auto &p : entities_)
            for (auto &b : p.second.behaviors)
                if (!b.isNative)
                    eng_behavior_release_script_self(g_eng_lua_vm, b.script);
    }
    entities_.clear();
    nextId_ = 1;
}

Entity *Scene::entity(uint32_t id)
{
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

const Entity *Scene::entity(uint32_t id) const
{
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

// ── Behavior operations ──

bool Scene::addBehavior(uint32_t entityId, const char *behaviorName, bool isNative,
                        const nlohmann::json &propertyOverrides)
{
    Entity *e = entity(entityId);
    if (!e || !behaviorName || behaviorName[0] == '\0')
        return false;
    if (isNative && std::strcmp(behaviorName, "Transform") == 0 && e->getTransform() != nullptr)
        return true;
    BehaviorSlot slot;
    slot.name = behaviorName;
    slot.isNative = isNative;
    if (!isNative)
    {
        slot.script.behavior = behaviorName;
        slot.script.started = false;
        slot.script.propertyOverrides = propertyOverrides.is_object() ? propertyOverrides : nlohmann::json::object();
        slot.script.luaInstanceRef = -1;
    }
    e->behaviors.push_back(std::move(slot));
    return true;
}

bool Scene::removeBehavior(uint32_t entityId, int index)
{
    Entity *e = entity(entityId);
    if (!e || index < 0 || index >= (int)e->behaviors.size())
        return false;
    if (!e->behaviors[index].isNative && g_eng_lua_vm)
        eng_behavior_release_script_self(g_eng_lua_vm, e->behaviors[index].script);
    e->behaviors.erase(e->behaviors.begin() + index);
    return true;
}

bool Scene::reorderBehavior(uint32_t entityId, int fromIndex, int toIndex)
{
    Entity *e = entity(entityId);
    if (!e)
        return false;
    int n = (int)e->behaviors.size();
    if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n || fromIndex == toIndex)
        return false;
    BehaviorSlot tmp = std::move(e->behaviors[fromIndex]);
    e->behaviors.erase(e->behaviors.begin() + fromIndex);
    e->behaviors.insert(e->behaviors.begin() + toIndex, std::move(tmp));
    return true;
}

// ── Backward-compat wrappers ──

bool Scene::addScript(uint32_t entityId, const char *behaviorName,
                      const nlohmann::json &propertyOverrides)
{
    if (behaviorName && eng_behavior_name_is_native_attachable(behaviorName))
        return addBehavior(entityId, behaviorName, true, propertyOverrides);
    return addBehavior(entityId, behaviorName, false, propertyOverrides);
}

bool Scene::removeScript(uint32_t entityId, int index)
{
    return removeBehavior(entityId, index);
}

bool Scene::reorderScript(uint32_t entityId, int fromIndex, int toIndex)
{
    return reorderBehavior(entityId, fromIndex, toIndex);
}

// ── Metadata setters ──

void Scene::setDrawOrder(uint32_t entityId, int order)
{
    Entity *e = entity(entityId);
    if (e)
        e->drawOrder = order;
}

void Scene::setUpdateOrder(uint32_t entityId, int order)
{
    Entity *e = entity(entityId);
    if (e)
        e->updateOrder = order;
}

void Scene::setName(uint32_t entityId, const char *name)
{
    Entity *e = entity(entityId);
    if (e && name)
        e->name = name;
}

void Scene::setActive(uint32_t entityId, bool active)
{
    Entity *e = entity(entityId);
    if (e)
        e->active = active;
}

bool Scene::setParent(uint32_t entityId, uint32_t parentId)
{
    if (entityId == parentId)
        return false;
    Entity *e = entity(entityId);
    if (!e || !entity(parentId))
        return false;
    uint32_t cur = parentId;
    while (cur != 0)
    {
        if (cur == entityId)
            return false;
        const Entity *p = entity(cur);
        if (!p)
            break;
        cur = p->parentId;
    }
    e->parentId = parentId;
    return true;
}

void Scene::removeParent(uint32_t entityId)
{
    Entity *e = entity(entityId);
    if (e)
        e->parentId = 0;
}

void Scene::setTransformField(uint32_t entityId, const char *field, float value)
{
    Entity *e = entity(entityId);
    if (!e || !field)
        return;
    Transform *t = e->getTransform();
    if (!t)
        return;
    if (!std::strcmp(field, "x"))
        t->x = value;
    else if (!std::strcmp(field, "y"))
        t->y = value;
    else if (!std::strcmp(field, "angle"))
        t->angle = value;
    else if (!std::strcmp(field, "vx"))
        t->vx = value;
    else if (!std::strcmp(field, "vy"))
        t->vy = value;
    else if (!std::strcmp(field, "sx"))
        t->sx = value;
    else if (!std::strcmp(field, "sy"))
        t->sy = value;
}

void Scene::resetScriptStartedFlags()
{
    for (auto &pair : entities_)
        for (auto &b : pair.second.behaviors)
            if (!b.isNative)
                b.script.started = false;
}

void Scene::releaseAllScriptLuaRefs(lua_State *L)
{
    if (!L)
        return;
    for (auto &pair : entities_)
        for (auto &b : pair.second.behaviors)
            if (!b.isNative)
                eng_behavior_release_script_self(L, b.script);
}

void Scene::invalidateAllBehaviorLuaRefs()
{
    for (auto &pair : entities_)
    {
        for (auto &b : pair.second.behaviors)
        {
            if (!b.isNative)
            {
                b.script.luaInstanceRef = -1;
                b.script.scriptVmGen = 0;
            }
        }
    }
}

// ── Iteration and world matrices ──

void Scene::forEachEntitySortedByDrawOrder(const std::function<void(Entity &)> &fn)
{
    std::vector<Entity *> order;
    order.reserve(entities_.size());
    for (auto &p : entities_)
        order.push_back(&p.second);
    std::sort(order.begin(), order.end(), [](Entity *a, Entity *b)
              {
        if (a->drawOrder != b->drawOrder)
            return a->drawOrder < b->drawOrder;
        return a->id < b->id; });
    for (Entity *e : order)
        fn(*e);
}

void Scene::forEachEntitySortedByUpdateOrder(const std::function<void(Entity &)> &fn)
{
    std::vector<Entity *> order;
    order.reserve(entities_.size());
    for (auto &p : entities_)
        order.push_back(&p.second);
    std::sort(order.begin(), order.end(), [](Entity *a, Entity *b)
              {
        if (a->updateOrder != b->updateOrder)
            return a->updateOrder < b->updateOrder;
        return a->id < b->id; });
    for (Entity *e : order)
        fn(*e);
}

std::unordered_map<uint32_t, Affine2D> Scene::computeWorldMatrices() const
{
    std::unordered_map<uint32_t, Affine2D> cache;
    cache.reserve(entities_.size());

    std::function<Affine2D(uint32_t)> resolve = [&](uint32_t id) -> Affine2D
    {
        auto cached = cache.find(id);
        if (cached != cache.end())
            return cached->second;
        auto it = entities_.find(id);
        if (it == entities_.end())
            return Affine2D::identity();
        const Entity &e = it->second;
        const Transform *t = e.getTransform();
        Affine2D local = t ? Affine2D::fromTRS(t->x, t->y, t->angle, t->sx, t->sy)
                           : Affine2D::identity();
        Affine2D world = (e.parentId != 0) ? resolve(e.parentId).multiply(local) : local;
        cache.emplace(id, world);
        return world;
    };

    for (const auto &pair : entities_)
        resolve(pair.first);

    return cache;
}

void Scene::worldDeltaToLocalPositionDelta(uint32_t entityId, float wdx, float wdy,
                                           float *outLx, float *outLy) const
{
    *outLx = wdx;
    *outLy = wdy;
    auto it = entities_.find(entityId);
    if (it == entities_.end())
        return;
    uint32_t parentId = it->second.parentId;
    if (parentId == 0)
        return;

    auto mats = computeWorldMatrices();
    auto pit = mats.find(parentId);
    if (pit == mats.end())
        return;

    const Affine2D &W = pit->second;
    float a = W.a, b = W.b, c = W.c, d = W.d;
    float det = a * d - b * c;
    if (std::fabs(det) < 1e-8f)
        return;

    float invDet = 1.f / det;
    float ia = d * invDet, ib = -b * invDet;
    float ic = -c * invDet, id = a * invDet;
    *outLx = ia * wdx + ib * wdy;
    *outLy = ic * wdx + id * wdy;
}

std::vector<const Entity *> Scene::entitiesSortedById() const
{
    std::vector<const Entity *> out;
    out.reserve(entities_.size());
    for (const auto &p : entities_)
        out.push_back(&p.second);
    std::sort(out.begin(), out.end(),
              [](const Entity *a, const Entity *b)
              { return a->id < b->id; });
    return out;
}
