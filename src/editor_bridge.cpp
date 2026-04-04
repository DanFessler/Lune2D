#include "editor_bridge.hpp"
#include "entity_registry.hpp"
#include "webview_host.hpp"

#include <cstdio>
#include <string>

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

} // namespace

void editor_bridge_publish_entity_snapshot(const EntityRegistry& registry) {
    std::string json = "[";
    bool        first = true;
    for (const EntityRecord& e : registry.records()) {
        if (!first)
            json += ',';
        first = false;
        json += '{';
        json += "\"id\":";
        appendJsonEscaped(json, e.id.c_str());
        json += ",\"name\":";
        appendJsonEscaped(json, e.name.c_str());
        char buf[192];
        std::snprintf(buf, sizeof(buf),
                      ",\"x\":%.8g,\"y\":%.8g,\"angle\":%.8g,\"vx\":%.8g,\"vy\":%.8g",
                      (double)e.x, (double)e.y, (double)e.angle, (double)e.vx, (double)e.vy);
        json += buf;
        json += '}';
    }
    json += ']';
    webview_host_publish_entities_json(json.c_str());
}
