#include "webview_host.hpp"

bool webview_host_init(SDL_Window* /*window*/, const char* /*web_root_abs*/) {
    return true;
}

void webview_host_shutdown() {}

void webview_host_on_window_resized(int /*pixel_w*/, int /*pixel_h*/) {}

void webview_host_set_game_rect_callback(WebViewGameRectFn /*fn*/, void* /*user*/) {}

void webview_host_poll_dom_layout() {}

void webview_host_get_layout_basis(int* basis_w, int* basis_h) {
    if (basis_w)
        *basis_w = 0;
    if (basis_h)
        *basis_h = 0;
}

bool webview_host_capture_composite_png(SDL_Window* /*window*/, const char* /*path_utf8*/) {
    return false;
}

void webview_host_publish_entities_json(const char* /*json_utf8*/) {}
