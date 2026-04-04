#include "scene_loader.hpp"

#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>

#include <fstream>

using json = nlohmann::json;

namespace {

void applyEntityJson(Scene& scene, uint32_t id, const json& ent) {
    if (ent.contains("drawOrder"))
        scene.setDrawOrder(id, ent["drawOrder"].get<int>());
    if (ent.contains("updateOrder"))
        scene.setUpdateOrder(id, ent["updateOrder"].get<int>());
    if (ent.contains("active") && !ent["active"].get<bool>())
        scene.setActive(id, false);

    if (ent.contains("transform") && ent["transform"].is_object()) {
        const auto& t = ent["transform"];
        if (t.contains("x"))
            scene.setTransformField(id, "x", t["x"].get<float>());
        if (t.contains("y"))
            scene.setTransformField(id, "y", t["y"].get<float>());
        if (t.contains("angle"))
            scene.setTransformField(id, "angle", t["angle"].get<float>());
        if (t.contains("vx"))
            scene.setTransformField(id, "vx", t["vx"].get<float>());
        if (t.contains("vy"))
            scene.setTransformField(id, "vy", t["vy"].get<float>());
        if (t.contains("sx"))
            scene.setTransformField(id, "sx", t["sx"].get<float>());
        if (t.contains("sy"))
            scene.setTransformField(id, "sy", t["sy"].get<float>());
    }

    if (ent.contains("scripts") && ent["scripts"].is_array()) {
        for (const auto& s : ent["scripts"]) {
            if (s.is_string())
                scene.addScript(id, s.get<std::string>().c_str());
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
        json tf;
        tf["x"]     = e.transform.x;
        tf["y"]     = e.transform.y;
        tf["angle"] = e.transform.angle;
        tf["vx"]    = e.transform.vx;
        tf["vy"]    = e.transform.vy;
        tf["sx"]    = e.transform.sx;
        tf["sy"]    = e.transform.sy;
        ent["transform"] = tf;
        json scripts = json::array();
        for (const auto& s : e.scripts)
            scripts.push_back(s.behavior);
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
