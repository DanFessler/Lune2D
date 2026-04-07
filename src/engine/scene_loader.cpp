#include "scene_loader.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <fstream>

#include "engine/lua/behavior_schema.hpp"

using json = nlohmann::json;

namespace {

void applyEntityJson(Scene& scene, uint32_t id, const json& ent) {
    if (ent.contains("drawOrder"))
        scene.setDrawOrder(id, ent["drawOrder"].get<int>());
    if (ent.contains("updateOrder"))
        scene.setUpdateOrder(id, ent["updateOrder"].get<int>());
    if (ent.contains("active") && !ent["active"].get<bool>())
        scene.setActive(id, false);

    const bool hasUnifiedBehaviors =
        ent.contains("behaviors") && ent["behaviors"].is_array();

    // Backward-compat: old format has "transform" as a separate object.
    if (!hasUnifiedBehaviors && ent.contains("transform") && ent["transform"].is_object()) {
        scene.addBehavior(id, "Transform", true);
        const auto& t = ent["transform"];
        if (t.contains("x"))     scene.setTransformField(id, "x",     t["x"].get<float>());
        if (t.contains("y"))     scene.setTransformField(id, "y",     t["y"].get<float>());
        if (t.contains("angle")) scene.setTransformField(id, "angle", t["angle"].get<float>());
        if (t.contains("vx"))    scene.setTransformField(id, "vx",    t["vx"].get<float>());
        if (t.contains("vy"))    scene.setTransformField(id, "vy",    t["vy"].get<float>());
        if (t.contains("sx"))    scene.setTransformField(id, "sx",    t["sx"].get<float>());
        if (t.contains("sy"))    scene.setTransformField(id, "sy",    t["sy"].get<float>());
    }

    // New format: "behaviors" array with unified behavior entries.
    if (hasUnifiedBehaviors) {
        for (const auto& b : ent["behaviors"]) {
            if (!b.is_object() || !b.contains("name") || !b["name"].is_string())
                continue;
            std::string name = b["name"].get<std::string>();
            bool isNative = b.value("isNative", false);
            if (isNative && name == "Transform") {
                scene.addBehavior(id, "Transform", true);
                if (b.contains("properties") && b["properties"].is_object()) {
                    Entity* ent = scene.entity(id);
                    if (ent) {
                        int idx = ent->getTransformIndex();
                        if (idx >= 0) {
                            const auto& p = b["properties"];
                            for (auto it = p.begin(); it != p.end(); ++it)
                                eng_behavior_slot_set_native_property(ent->behaviors[idx], it.key().c_str(), it.value());
                        }
                    }
                }
            } else {
                json props = b.value("properties", json::object());
                if (!props.is_object()) props = json::object();
                scene.addScript(id, name.c_str(), props);
            }
        }
    }

    // Legacy "scripts" array (backward-compat).
    if (!hasUnifiedBehaviors && ent.contains("scripts") && ent["scripts"].is_array()) {
        for (const auto& s : ent["scripts"]) {
            if (s.is_string())
                scene.addScript(id, s.get<std::string>().c_str());
            else if (s.is_object()) {
                if (!s.contains("behavior") || !s["behavior"].is_string()) {
                    SDL_Log("eng_load_scene: script object missing behavior string");
                    continue;
                }
                std::string    beh = s["behavior"].get<std::string>();
                json           o   = s.value("properties", json::object());
                if (!o.is_object())
                    o = json::object();
                scene.addScript(id, beh.c_str(), o);
            }
        }
    }
}

} // namespace

bool eng_load_scene(Scene& scene, const std::string& jsonPath) {
    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        SDL_Log("eng_load_scene: cannot open %s", jsonPath.c_str());
        return false;
    }

    json doc;
    try {
        doc = json::parse(file);
    } catch (const json::parse_error& e) {
        SDL_Log("eng_load_scene: JSON parse error in %s: %s", jsonPath.c_str(), e.what());
        return false;
    }

    if (!doc.contains("entities") || !doc["entities"].is_array()) {
        SDL_Log("eng_load_scene: missing 'entities' array in %s", jsonPath.c_str());
        return false;
    }

    scene.clear();

    const auto& entities = doc["entities"];
    bool        explicitIds = false;
    if (!entities.empty()) {
        explicitIds = entities[0].contains("id");
        for (const auto& ent : entities) {
            if (ent.contains("id") != explicitIds) {
                SDL_Log("eng_load_scene: mixed 'id' / no-id entities in %s", jsonPath.c_str());
                return false;
            }
        }
    }

    if (explicitIds) {
        for (const auto& ent : entities) {
            uint32_t    id   = ent["id"].get<uint32_t>();
            std::string name = ent.value("name", "Entity");
            scene.insertEntityWithId(id, std::move(name));
        }
        for (const auto& ent : entities) {
            uint32_t id = ent["id"].get<uint32_t>();
            applyEntityJson(scene, id, ent);
        }
        for (const auto& ent : entities) {
            if (!ent.contains("parentId"))
                continue;
            uint32_t id  = ent["id"].get<uint32_t>();
            uint32_t pid = ent["parentId"].get<uint32_t>();
            if (pid != 0)
                scene.setParent(id, pid);
        }
    } else {
        for (const auto& ent : entities) {
            std::string name = ent.value("name", "Entity");
            uint32_t    id   = scene.spawn(name);
            applyEntityJson(scene, id, ent);
        }
    }

    SDL_Log("eng_load_scene: loaded %zu entities from %s",
            entities.size(), jsonPath.c_str());
    return true;
}

bool eng_save_scene(const Scene& scene, const std::string& jsonPath) {
    json        doc = json::object();
    json        arr = json::array();
    std::vector<const Entity*> sorted = scene.entitiesSortedById();
    for (const Entity* ep : sorted) {
        const Entity& e = *ep;
        json          ent;
        ent["id"]          = e.id;
        ent["name"]        = e.name;
        ent["drawOrder"]   = e.drawOrder;
        ent["updateOrder"] = e.updateOrder;
        if (!e.active)
            ent["active"] = false;
        if (e.parentId != 0)
            ent["parentId"] = e.parentId;

        // Save using the new "behaviors" array format.
        json behaviors = json::array();
        for (const auto& b : e.behaviors) {
            json slot;
            slot["name"] = b.name;
            if (b.isNative) {
                slot["isNative"] = true;
                json props = eng_behavior_slot_native_properties(b);
                if (!props.empty())
                    slot["properties"] = props;
            } else {
                json saveProps = eng_behavior_overrides_for_save(b.script.behavior.c_str(),
                                                                 b.script.propertyOverrides);
                if (!saveProps.empty())
                    slot["properties"] = saveProps;
            }
            behaviors.push_back(slot);
        }
        ent["behaviors"] = behaviors;

        // Also write legacy "transform" and "scripts" fields for backward compat during transition.
        const Transform *t = e.getTransform();
        if (t) {
            json tf;
            tf["x"]     = t->x;
            tf["y"]     = t->y;
            tf["angle"] = t->angle;
            tf["vx"]    = t->vx;
            tf["vy"]    = t->vy;
            tf["sx"]    = t->sx;
            tf["sy"]    = t->sy;
            ent["transform"] = tf;
        }
        json scripts = json::array();
        for (const auto& b : e.behaviors) {
            if (b.isNative) continue;
            json saveProps = eng_behavior_overrides_for_save(b.script.behavior.c_str(),
                                                             b.script.propertyOverrides);
            if (saveProps.empty())
                scripts.push_back(b.script.behavior);
            else {
                json s;
                s["behavior"]   = b.script.behavior;
                s["properties"] = saveProps;
                scripts.push_back(s);
            }
        }
        ent["scripts"] = scripts;

        arr.push_back(ent);
    }
    doc["entities"] = arr;

    std::ofstream out(jsonPath);
    if (!out.is_open()) {
        SDL_Log("eng_save_scene: cannot write %s", jsonPath.c_str());
        return false;
    }
    out << doc.dump(2) << "\n";
    if (!out.good()) {
        SDL_Log("eng_save_scene: write failed %s", jsonPath.c_str());
        return false;
    }
    SDL_Log("eng_save_scene: wrote %zu entities to %s", sorted.size(), jsonPath.c_str());
    return true;
}
