#pragma once

#include <SDL3/SDL.h>

#include <cstdint>

struct SDL_Renderer;

// Called from the web UI with rect + UI-space dimensions from getBoundingClientRect().
// Native maps this UI space to renderer output pixels.
using WebViewGameRectFn =
    void (*)(int x, int y, int w, int h, int ui_space_w, int ui_space_h, void *user);

// Attach a transparent HTML overlay to the SDL window (platform-specific).
// web_root: absolute directory containing index.html (file:// load), or an http(s) URL
// (e.g. http://127.0.0.1:5173/ from `npm run dev` in /web).
bool webview_host_init(SDL_Window *window, const char *web_root_abs);

void webview_host_shutdown();

void webview_host_on_window_resized(int pixel_w, int pixel_h);

void webview_host_set_game_rect_callback(WebViewGameRectFn fn, void *user);

// Toggle editor overlay visibility. Returns true when visible after toggle.
bool webview_host_toggle_visibility();

// macOS: false while WKWebView overlay is hidden. Stub: false (no overlay).
bool webview_host_web_overlay_visible();

// Pull #game-surface rect from the page via evaluateJavaScript (works when window.webkit.postMessage does not).
void webview_host_poll_dom_layout();

// WKWebView bounds in layout points (same space as getBoundingClientRect). Stub leaves *w=*h=0.
void webview_host_get_layout_basis(int *basis_w, int *basis_h);

// Full-window letterbox clear, then viewport + scale so 0..lu_w/lu_h maps to game_rect.
// If out_lu_w/h non-null, filled with Luau coordinate extent (inset size or full logical size).
void webview_apply_game_viewport(SDL_Renderer *renderer,
                                 SDL_Window *window,
                                 int logical_w,
                                 int logical_h,
                                 int game_x,
                                 int game_y,
                                 int game_w,
                                 int game_h,
                                 int ui_space_w,
                                 int ui_space_h,
                                 bool native_pct_rect,
                                 int *out_lu_w = nullptr,
                                 int *out_lu_h = nullptr);

// Map a window-relative mouse position to Luau / game draw coordinates (0..lu_w/h).
// Returns false if the point is outside the game inset (same basis as webview_apply_game_viewport).
bool webview_window_mouse_to_luau(SDL_Window *window,
                                  SDL_Renderer *renderer,
                                  int logical_w,
                                  int logical_h,
                                  int game_x,
                                  int game_y,
                                  int game_w,
                                  int game_h,
                                  int ui_space_w,
                                  int ui_space_h,
                                  bool native_pct_rect,
                                  float window_mouse_x,
                                  float window_mouse_y,
                                  float *out_lu_x,
                                  float *out_lu_y);

// macOS: capture the full composited window (SDL + WebKit) to a PNG. Stub returns false.
bool webview_host_capture_composite_png(SDL_Window *window, const char *path_utf8);

// Push entity snapshot JSON (UTF-8) to the web UI: window.__engineOnEntities(payload).
// Safe when the webview is absent: stub is a no-op.
void webview_host_publish_entities_json(const char *json_utf8);

// Calls window.__engineSelectEntity(id) with a numeric id, or null when id == 0. Stub is a no-op.
void webview_host_notify_selected_entity(std::uint32_t entity_id);

// Absolute path to the directory containing *.lua scripts (e.g. ".../lua").
void webview_host_set_lua_workspace(const char *lua_dir_abs_utf8);

// Host callbacks for the JS "engineScript" bridge (Lua editor tooling). Stubs are no-ops.
void webview_host_set_script_controls(void (*on_reload_request)(void),
                                      void (*on_set_paused)(bool paused),
                                      void (*on_start_sim_request)(bool capture_editor_scene_snapshot));

/// Runs synchronously when the web UI sends `reloadBehaviors` (before the ACK). No-op if unset.
void webview_host_set_behaviors_reload_fn(void (*reload_fn)(void));

/// macOS: default directory for Save Scene As (`…/lua/scenes`). Stubs ignore.
void webview_host_set_macos_scenes_directory_for_save_panel(const char *scenes_dir_abs_utf8);

/// macOS: File menu (Save Scene ⌘S, Save Scene As… ⇧⌘S) and app Quit (⌘Q). Stubs are no-ops.
void webview_host_install_macos_app_menu(void (*on_quit_requested)(void),
                                         void (*on_save_scene_default)(void),
                                         void (*on_save_scene_as)(const char *absolute_path_utf8));

/// Clear SDL keyboard state when UI focus moves to the web overlay (macOS) so key-ups are not
/// missed. Safe no-op on stubs or when no overlay. Call once per frame after SDL_PollEvent.
void webview_host_sync_sdl_keyboard_state(void);
