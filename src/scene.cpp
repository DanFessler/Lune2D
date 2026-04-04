#include "scene.hpp"

#include <algorithm>
#include <cstring>

uint32_t Scene::spawn(std::string name) {
    uint32_t id = nextId_++;
    Entity   e;
    e.id   = id;
    e.name = name.empty() ? "Entity" : std::move(name);
    entities_.emplace(id, std::move(e));
    return id;
}

void Scene::insertEntityWithId(uint32_t id, std::string name) {
    Entity e;
    e.id   = id;
    e.name = name.empty() ? "Entity" : std::move(name);
    entities_[id] = std::move(e);
    if (id >= nextId_)
        nextId_ = id + 1;
}

void Scene::destroy(uint32_t id) {
    entities_.erase(id);
}

void Scene::clear() {
    entities_.clear();
    nextId_ = 1;
}

Entity* Scene::entity(uint32_t id) {
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

const Entity* Scene::entity(uint32_t id) const {
    auto it = entities_.find(id);
    return it == entities_.end() ? nullptr : &it->second;
}

bool Scene::addScript(uint32_t entityId, const char* behaviorName) {
    Entity* e = entity(entityId);
    if (!e || !behaviorName || behaviorName[0] == '\0')
        return false;
    ScriptInstance s;
    s.behavior = behaviorName;
    s.started  = false;
    e->scripts.push_back(std::move(s));
    return true;
}

void Scene::setDrawOrder(uint32_t entityId, int order) {
    Entity* e = entity(entityId);
    if (e)
        e->drawOrder = order;
}

void Scene::setUpdateOrder(uint32_t entityId, int order) {
    Entity* e = entity(entityId);
    if (e)
        e->updateOrder = order;
}

void Scene::setName(uint32_t entityId, const char* name) {
    Entity* e = entity(entityId);
    if (e && name)
        e->name = name;
}

void Scene::setActive(uint32_t entityId, bool active) {
    Entity* e = entity(entityId);
    if (e)
        e->active = active;
}

bool Scene::removeScript(uint32_t entityId, int index) {
    Entity* e = entity(entityId);
    if (!e || index < 0 || index >= (int)e->scripts.size())
        return false;
    e->scripts.erase(e->scripts.begin() + index);
    return true;
}

bool Scene::reorderScript(uint32_t entityId, int fromIndex, int toIndex) {
    Entity* e = entity(entityId);
    if (!e)
        return false;
    int n = (int)e->scripts.size();
    if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n || fromIndex == toIndex)
        return false;
    ScriptInstance tmp = std::move(e->scripts[fromIndex]);
    e->scripts.erase(e->scripts.begin() + fromIndex);
    e->scripts.insert(e->scripts.begin() + toIndex, std::move(tmp));
    return true;
}

bool Scene::setParent(uint32_t entityId, uint32_t parentId) {
    if (entityId == parentId)
        return false;
    Entity* e = entity(entityId);
    if (!e || !entity(parentId))
        return false;
    // Cycle guard: walk from parentId up; if we reach entityId, it's a cycle.
    uint32_t cur = parentId;
    while (cur != 0) {
        if (cur == entityId)
            return false;
        const Entity* p = entity(cur);
        if (!p)
            break;
        cur = p->parentId;
    }
    e->parentId = parentId;
    return true;
}

void Scene::removeParent(uint32_t entityId) {
    Entity* e = entity(entityId);
    if (e)
        e->parentId = 0;
}

void Scene::setTransformField(uint32_t entityId, const char* field, float value) {
    Entity* e = entity(entityId);
    if (!e || !field)
        return;
    Transform& t = e->transform;
    if (!std::strcmp(field, "x"))
        t.x = value;
    else if (!std::strcmp(field, "y"))
        t.y = value;
    else if (!std::strcmp(field, "angle"))
        t.angle = value;
    else if (!std::strcmp(field, "vx"))
        t.vx = value;
    else if (!std::strcmp(field, "vy"))
        t.vy = value;
    else if (!std::strcmp(field, "sx"))
        t.sx = value;
    else if (!std::strcmp(field, "sy"))
        t.sy = value;
}

void Scene::resetScriptStartedFlags() {
    for (auto& pair : entities_) {
        for (auto& s : pair.second.scripts)
            s.started = false;
    }
}

void Scene::forEachEntitySortedByDrawOrder(const std::function<void(Entity&)>& fn) {
    std::vector<Entity*> order;
    order.reserve(entities_.size());
    for (auto& p : entities_)
        order.push_back(&p.second);
    std::sort(order.begin(), order.end(), [](Entity* a, Entity* b) {
        if (a->drawOrder != b->drawOrder)
            return a->drawOrder < b->drawOrder;
        return a->id < b->id;
    });
    for (Entity* e : order)
        fn(*e);
}

void Scene::forEachEntitySortedByUpdateOrder(const std::function<void(Entity&)>& fn) {
    std::vector<Entity*> order;
    order.reserve(entities_.size());
    for (auto& p : entities_)
        order.push_back(&p.second);
    std::sort(order.begin(), order.end(), [](Entity* a, Entity* b) {
        if (a->updateOrder != b->updateOrder)
            return a->updateOrder < b->updateOrder;
        return a->id < b->id;
    });
    for (Entity* e : order)
        fn(*e);
}

std::unordered_map<uint32_t, Affine2D> Scene::computeWorldMatrices() const {
    std::unordered_map<uint32_t, Affine2D> cache;
    cache.reserve(entities_.size());

    // Recursive lambda walks up parent chain, caching results.
    std::function<Affine2D(uint32_t)> resolve = [&](uint32_t id) -> Affine2D {
        auto cached = cache.find(id);
        if (cached != cache.end())
            return cached->second;
        auto it = entities_.find(id);
        if (it == entities_.end())
            return Affine2D::identity();
        const Entity& e   = it->second;
        const Transform& t = e.transform;
        Affine2D local = Affine2D::fromTRS(t.x, t.y, t.angle, t.sx, t.sy);
        Affine2D world = (e.parentId != 0) ? resolve(e.parentId).multiply(local) : local;
        cache.emplace(id, world);
        return world;
    };

    for (const auto& pair : entities_)
        resolve(pair.first);

    return cache;
}

std::vector<const Entity*> Scene::entitiesSortedById() const {
    std::vector<const Entity*> out;
    out.reserve(entities_.size());
    for (const auto& p : entities_)
        out.push_back(&p.second);
    std::sort(out.begin(), out.end(),
              [](const Entity* a, const Entity* b) { return a->id < b->id; });
    return out;
}
