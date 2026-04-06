#include "editor_bridge.hpp"
#include "engine/lua/behavior_schema.hpp"
#include "scene.hpp"
#include "webview_host.hpp"

#include <cstdio>
#include <string>

#include <nlohmann/json.hpp>

namespace {

void appendJsonEscaped(std::string& out, const char* s) {
    out.push_back('"');
    if (s) {
        for (; *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') {
                out += '\\';
                out.push_back((char)c);
            } else if (c == '\n') {
                out += "\\n";
            } else if (c == '\r') {
                out += "\\r";
            } else if (c == '\t') {
                out += "\\t";
            } else {
                out.push_back((char)c);
            }
        }
    }
    out.push_back('"');
}

void appendTransformComponent(std::string& json, const Transform& t) {
    char buf[384];
    std::snprintf(buf, sizeof(buf),
                  "{\"type\":\"Transform\",\"x\":%.8g,\"y\":%.8g,\"angle\":%.8g,"
                  "\"vx\":%.8g,\"vy\":%.8g,\"sx\":%.8g,\"sy\":%.8g}",
                  (double)t.x, (double)t.y, (double)t.angle,
                  (double)t.vx, (double)t.vy, (double)t.sx, (double)t.sy);
    json += buf;
}

void appendScriptComponent(std::string& json, const ScriptInstance& s) {
    nlohmann::json row   = nlohmann::json::object();
    row["type"]          = "Script";
    row["behavior"]      = s.behavior;
    row["properties"]    = s.propertyOverrides;
    row["propertyValues"] =
        eng_behavior_merge_properties(s.behavior.c_str(), s.propertyOverrides);
    nlohmann::json sch = eng_behavior_schema_to_editor_json(s.behavior.c_str());
    if (!sch.is_null())
        row["propertySchema"] = std::move(sch);
    json += row.dump();
}

} // namespace

void editor_bridge_publish_scene_snapshot(const Scene& scene) {
    std::string  json = "[";
    bool         firstEntity = true;
    const auto   sorted      = scene.entitiesSortedById();
    char         idBuf[32];

    for (const Entity* ep : sorted) {
        const Entity& e = *ep;
        if (!firstEntity)
            json += ',';
        firstEntity = false;

        std::snprintf(idBuf, sizeof(idBuf), "%u", (unsigned)e.id);
        json += '{';
        json += "\"id\":";
        appendJsonEscaped(json, idBuf);
        json += ",\"name\":";
        appendJsonEscaped(json, e.name.c_str());
        json += e.active ? ",\"active\":true" : ",\"active\":false";
        json += ",\"drawOrder\":";
        char oBuf[32];
        std::snprintf(oBuf, sizeof(oBuf), "%d", e.drawOrder);
        json += oBuf;
        json += ",\"updateOrder\":";
        char uBuf[32];
        std::snprintf(uBuf, sizeof(uBuf), "%d", e.updateOrder);
        json += uBuf;
        if (e.parentId != 0) {
            char pBuf[32];
            std::snprintf(pBuf, sizeof(pBuf), ",\"parentId\":\"%u\"", (unsigned)e.parentId);
            json += pBuf;
        }
        json += ",\"components\":[";


        appendTransformComponent(json, e.transform);
        for (const ScriptInstance& sc : e.scripts) {
            json += ',';
            appendScriptComponent(json, sc);
        }

        json += "]}";
    }
    json += ']';
    webview_host_publish_entities_json(json.c_str());
}
