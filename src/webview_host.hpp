#pragma once

#include <SDL3/SDL.h>

struct SDL_Renderer;

// Called from the web UI with rect + UI-space dimensions from getBoundingClientRect().
// Native maps this UI space to renderer output pixels.
using WebViewGameRectFn =
    void (*)(int x, int y, int w, int h, int ui_space_w, int ui_space_h, void* user);

// Attach a transparent HTML overlay to the SDL window (platform-specific).
// web_root: absolute directory containing index.html (file:// load), or an http(s) URL
// (e.g. http://127.0.0.1:5173/ from `npm run dev` in /web).
bool webview_host_init(SDL_Window* window, const char* web_root_abs);

void webview_host_shutdown();

void webview_host_on_window_resized(int pixel_w, int pixel_h);

void webview_host_set_game_rect_callback(WebViewGameRectFn fn, void* user);

// Pull #game-surface rect from the page via evaluateJavaScript (works when window.webkit.postMessage does not).
void webview_host_poll_dom_layout();

// WKWebView bounds in layout points (same space as getBoundingClientRect). Stub leaves *w=*h=0.
void webview_host_get_layout_basis(int* basis_w, int* basis_h);

// Full-window letterbox clear, then viewport + scale so 0..lu_w/lu_h maps to game_rect.
// If out_lu_w/h non-null, filled with Luau coordinate extent (inset size or full logical size).
void webview_apply_game_viewport(SDL_Renderer* renderer,
                                 SDL_Window* window,
                                 int logical_w,
                                 int logical_h,
                                 int game_x,
                                 int game_y,
                                 int game_w,
                                 int game_h,
                                 int ui_space_w,
                                 int ui_space_h,
                                 bool native_pct_rect,
                                 int* out_lu_w = nullptr,
                                 int* out_lu_h = nullptr);

// macOS: capture the full composited window (SDL + WebKit) to a PNG. Stub returns false.
bool webview_host_capture_composite_png(SDL_Window* window, const char* path_utf8);

// Push entity snapshot JSON (UTF-8) to the web UI: window.__engineOnEntities(payload).
// Safe when the webview is absent: stub is a no-op.
void webview_host_publish_entities_json(const char* json_utf8);

// Absolute path to the directory containing *.lua scripts (e.g. ".../lua").
void webview_host_set_lua_workspace(const char* lua_dir_abs_utf8);

// Host callbacks for the JS "engineScript" bridge (Lua editor tooling). Stubs are no-ops.
void webview_host_set_script_controls(void (*on_reload_request)(void),
                                      void (*on_set_paused)(bool paused),
                                      void (*on_start_sim_request)(void));
