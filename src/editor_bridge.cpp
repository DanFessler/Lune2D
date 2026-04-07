#include "editor_bridge.hpp"
#include "engine/lua/behavior_schema.hpp"
#include "engine/lua/lua_runtime.hpp"
#include "scene.hpp"
#include "webview_host.hpp"

#include <cstdio>
#include <string>
#include <unordered_set>

#include <lua.h>
#include <nlohmann/json.hpp>

namespace
{

    void appendJsonEscaped(std::string &out, const char *s)
    {
        out.push_back('"');
        if (s)
        {
            for (; *s; ++s)
            {
                unsigned char c = (unsigned char)*s;
                if (c == '"' || c == '\\')
                {
                    out += '\\';
                    out.push_back((char)c);
                }
                else if (c == '\n')
                {
                    out += "\\n";
                }
                else if (c == '\r')
                {
                    out += "\\r";
                }
                else if (c == '\t')
                {
                    out += "\\t";
                }
                else
                {
                    out.push_back((char)c);
                }
            }
        }
        out.push_back('"');
    }

    void appendBehaviorComponent(std::string &json, const BehaviorSlot &b,
                                 const std::unordered_set<std::string> &editorPairs)
    {
        bool hasPair = editorPairs.count(b.name) > 0;
        nlohmann::json row = nlohmann::json::object();
        row["type"] = "Behavior";
        row["name"] = b.name;
        row["isNative"] = b.isNative;
        row["hasEditorPair"] = hasPair;
        if (b.isNative)
        {
            nlohmann::json props = eng_behavior_slot_native_properties(b);
            if (!props.empty())
                row["properties"] = props;
            nlohmann::json values = eng_behavior_merge_properties(b.name.c_str(), props);
            row["propertyValues"] = values.empty() ? props : values;
        }
        else
        {
            row["properties"] = b.script.propertyOverrides;
            row["propertyValues"] =
                eng_behavior_merge_properties(b.script.behavior.c_str(), b.script.propertyOverrides);
        }
        nlohmann::json sch = eng_behavior_schema_to_editor_json(b.name.c_str());
        if (!sch.is_null())
            row["propertySchema"] = std::move(sch);
        json += row.dump();
    }

} // namespace

static std::unordered_set<std::string> collectEditorBehaviorNames()
{
    std::unordered_set<std::string> out;
    lua_State *L = g_eng_lua_vm;
    if (!L) return out;
    lua_getglobal(L, "_EDITOR_BEHAVIORS");
    if (!lua_istable(L, -1)) { lua_pop(L, 1); return out; }
    lua_pushnil(L);
    while (lua_next(L, -2) != 0)
    {
        if (lua_type(L, -2) == LUA_TSTRING)
            out.insert(lua_tostring(L, -2));
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
    return out;
}

void editor_bridge_publish_scene_snapshot(const Scene &scene)
{
    auto editorPairs = collectEditorBehaviorNames();

    std::string json = "[";
    bool firstEntity = true;
    const auto sorted = scene.entitiesSortedById();
    char idBuf[32];

    for (const Entity *ep : sorted)
    {
        const Entity &e = *ep;
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
        if (e.parentId != 0)
        {
            char pBuf[32];
            std::snprintf(pBuf, sizeof(pBuf), ",\"parentId\":\"%u\"", (unsigned)e.parentId);
            json += pBuf;
        }
        json += ",\"components\":[";

        bool firstComp = true;
        for (const BehaviorSlot &b : e.behaviors)
        {
            if (!firstComp)
                json += ',';
            firstComp = false;
            appendBehaviorComponent(json, b, editorPairs);
        }

        json += "]}";
    }
    json += ']';
    webview_host_publish_entities_json(json.c_str());
}
