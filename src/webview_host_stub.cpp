#include "webview_host.hpp"

bool webview_host_init(SDL_Window* /*window*/, const char* /*web_root_abs*/) {
    return true;
}

void webview_host_shutdown() {}

void webview_host_on_window_resized(int /*pixel_w*/, int /*pixel_h*/) {}

void webview_host_set_game_rect_callback(WebViewGameRectFn /*fn*/, void* /*user*/) {}

bool webview_host_toggle_visibility() {
    return false;
}

bool webview_host_web_overlay_visible() {
    return false;
}

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

void webview_host_set_lua_workspace(const char* /*lua_dir_abs_utf8*/) {}

void webview_host_set_script_controls(void (* /*on_reload_request*/)(void),
                                      void (* /*on_set_paused*/)(bool),
                                      void (* /*on_start_sim_request*/)(bool)) {}

void webview_host_set_behaviors_reload_fn(void (* /*reload_fn*/)(void)) {}

void webview_host_set_macos_scenes_directory_for_save_panel(const char* /*scenes_dir_abs_utf8*/) {}

void webview_host_install_macos_app_menu(void (* /*on_quit*/)(void),
                                         void (* /*on_save_default*/)(void),
                                         void (* /*on_save_as*/)(const char*)) {}

void webview_host_sync_sdl_keyboard_state(void) {}
