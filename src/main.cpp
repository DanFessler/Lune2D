#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <lua.h>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

#include "editor_bridge.hpp"
#include "editor_state.hpp"
#include "engine/camera.hpp"
#include "engine/lua/lua_editor_input.hpp"
#include "engine/constants.hpp"
#include "engine/engine.hpp"
#include "engine/game_keyboard.hpp"
#include "engine/lua/lua_runtime.hpp"
#include "engine/lua/lua_vm_lifecycle.hpp"
#include "engine/lua/script_host.hpp"
#include "engine/platform_paths.hpp"
#include "engine/scene_loader.hpp"
#include "scene.hpp"
#include "webview_host.hpp"

static std::atomic<bool> s_script_reload_requested{false};
static std::atomic<bool> s_script_paused{false};
/// Packed: bit0 = start sim this frame, bit1 = copy scene to editor snapshot before HUD play.
static std::atomic<uint32_t> s_pending_start_sim{0};
static constexpr uint32_t kStartSimPendingBit = 1u << 0;
static constexpr uint32_t kStartSimSnapshotBit = 1u << 1;

static Scene s_scene_snapshot_before_play;
static bool s_have_scene_snapshot_before_play = false;

static lua_State *s_lua_for_behaviors_reload = nullptr;

static void on_script_reload_request()
{
    eng_editor_set_sim_ui_state(EngSimUiState::Stopped);
    eng_camera_state().activeCameraEntityId = 0;
    s_script_reload_requested.store(true, std::memory_order_relaxed);
}

static void on_script_set_paused(bool paused)
{
    eng_editor_set_sim_ui_state(paused ? EngSimUiState::Paused : EngSimUiState::Playing);
    s_script_paused.store(paused, std::memory_order_relaxed);
}

static void on_script_start_sim_request(bool capture_editor_scene_snapshot)
{
    eng_editor_set_sim_ui_state(EngSimUiState::Playing);
    uint32_t v = kStartSimPendingBit;
    if (capture_editor_scene_snapshot)
        v |= kStartSimSnapshotBit;
    s_pending_start_sim.store(v, std::memory_order_release);
}

static int s_ui_game_x = 0, s_ui_game_y = 0, s_ui_game_w = 0, s_ui_game_h = 0;
static int s_ui_space_w = 0, s_ui_space_h = 0;

static void on_web_game_rect(int x, int y, int w, int h, int ui_space_w, int ui_space_h,
                             void * /*user*/)
{
    s_ui_game_x = x;
    s_ui_game_y = y;
    s_ui_game_w = w;
    s_ui_game_h = h;
    s_ui_space_w = ui_space_w;
    s_ui_space_h = ui_space_h;
}

static int s_capture_after_frames = -1;
static const char *s_capture_path = nullptr;
static bool s_native_game_rect_pct = false;
static std::string s_cli_project_dir;
static bool s_run_visual_tests = false;
static int s_visual_test_after_frames = 90;


static bool main_layout_dimensions(int *layout_w, int *layout_h)
{
    if (!layout_w || !layout_h)
        return false;
    *layout_w = SCREEN_W;
    *layout_h = SCREEN_H;
    if (!webview_host_web_overlay_visible())
        SDL_GetWindowSize(g_eng.window, layout_w, layout_h);
    return true;
}

static bool main_window_mouse_to_lu(float window_mx, float window_my, float *lu_x, float *lu_y,
                                    bool clamp_to_game = false)
{
    int layout_w = SCREEN_W, layout_h = SCREEN_H;
    main_layout_dimensions(&layout_w, &layout_h);
    return webview_window_mouse_to_luau(
        g_eng.window, g_eng.renderer,
        layout_w, layout_h,
        s_ui_game_x, s_ui_game_y, s_ui_game_w, s_ui_game_h,
        s_ui_space_w, s_ui_space_h, s_native_game_rect_pct,
        window_mx, window_my, lu_x, lu_y, clamp_to_game);
}

// Native pick/drag removed — handled by engine Luau `lua/editor/Transform.lua` via editorInput APIs.

static void parse_cli(int argc, char **argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--capture-after-frames") == 0 && i + 1 < argc)
        {
            s_capture_after_frames = std::atoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--capture-window-png") == 0 && i + 1 < argc)
        {
            s_capture_path = argv[++i];
        }
        else if (std::strcmp(argv[i], "--native-game-rect") == 0)
        {
            s_native_game_rect_pct = true;
        }
        else if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc)
        {
            s_cli_project_dir = argv[++i];
        }
        else if (std::strcmp(argv[i], "--run-visual-tests") == 0)
        {
            s_run_visual_tests = true;
        }
        else if (std::strcmp(argv[i], "--visual-test-after-frames") == 0 && i + 1 < argc)
        {
            s_visual_test_after_frames = std::atoi(argv[++i]);
        }
    }
    if (s_capture_path && s_capture_after_frames < 0)
        s_capture_after_frames = 180;
    if (s_run_visual_tests && s_visual_test_after_frames < 1)
        s_visual_test_after_frames = 90;
}

static bool eng_run_visual_tests_lua(lua_State *L)
{
    lua_getglobal(L, "_runVisualTests");
    if (!lua_isfunction(L, -1))
    {
        SDL_Log("visual tests: _runVisualTests is not a function");
        lua_pop(L, 1);
        return false;
    }
    if (lua_pcall(L, 0, 1, 0) != LUA_OK)
    {
        SDL_Log("visual tests: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    bool ok = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return ok;
}

static std::string s_engine_lua_dir;
static std::string s_project_lua_dir;
static std::string s_default_scene_path;

static void reload_behaviors_from_web_ui()
{
    if (s_lua_for_behaviors_reload)
        eng_reload_behaviors(s_lua_for_behaviors_reload, s_engine_lua_dir, s_project_lua_dir);
}

static lua_State *create_lua_vm_only()
{
    return eng_create_lua_vm(s_engine_lua_dir, s_project_lua_dir);
}

static void load_default_scene_file_into_g_scene()
{
    g_scene.clear();
    eng_load_scene(g_scene, s_default_scene_path);
}

static void native_request_quit()
{
    g_eng.quit = true;
}

static void native_save_scene_default()
{
    if (!eng_save_scene(g_scene, s_default_scene_path))
        SDL_Log("eng_save_scene failed: %s", s_default_scene_path.c_str());
}

static void native_save_scene_as(const char *pathUtf8)
{
    if (!pathUtf8 || !eng_save_scene(g_scene, std::string(pathUtf8)))
        SDL_Log("eng_save_scene (Save As) failed");
}

static lua_State *create_vm_and_load_scene()
{
    lua_State *L = create_lua_vm_only();
    if (!L)
        return nullptr;
    load_default_scene_file_into_g_scene();
    return L;
}

int main(int argc, char *argv[])
{
    parse_cli(argc, argv);
    eng_set_visual_test_skip_editor_overlays(s_run_visual_tests);
    srand(static_cast<unsigned>(time(nullptr)));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    g_eng.window = SDL_CreateWindow("Lune2D", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE);
    g_eng.renderer = SDL_CreateRenderer(g_eng.window, nullptr);
    if (!g_eng.window || !g_eng.renderer)
    {
        SDL_Log("Window/Renderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(g_eng.renderer, 1);
    g_eng.audio.init();
    eng_game_keyboard_init(&g_eng);

    webview_host_set_game_rect_callback(on_web_game_rect, nullptr);
    std::string repoLua = std::string(SDL_GetBasePath()) + "../lua";
    s_engine_lua_dir = repoLua;
    if (!s_cli_project_dir.empty())
    {
        s_project_lua_dir = s_cli_project_dir;
        if (!s_project_lua_dir.empty() && s_project_lua_dir.back() != '/')
            s_project_lua_dir += '/';
    }
    else
        s_project_lua_dir = std::string(SDL_GetBasePath()) + "../default-project";
    eng_set_project_lua_dir(s_project_lua_dir);
    webview_host_set_lua_workspace(s_project_lua_dir.c_str());
    webview_host_set_lua_engine_workspace(s_engine_lua_dir.c_str());
    s_default_scene_path = s_project_lua_dir + "/scenes/default.scene.json";

    webview_host_set_script_controls(on_script_reload_request, on_script_set_paused,
                                     on_script_start_sim_request);
    {
        std::string webRoot = eng_web_ui_root_path();
        if (!webview_host_init(g_eng.window, webRoot.c_str()))
            SDL_Log("Web overlay init failed (tried %s)", webRoot.c_str());
    }

    webview_host_set_macos_scenes_directory_for_save_panel((s_project_lua_dir + "/scenes").c_str());
    webview_host_install_macos_app_menu(native_request_quit, native_save_scene_default,
                                        native_save_scene_as);

    lua_State *L = create_vm_and_load_scene();
    if (!L)
    {
        SDL_Quit();
        return 1;
    }

    eng_lua_bind_main_vm(L);

    if (s_run_visual_tests)
    {
        lua_getglobal(L, "require");
        lua_pushstring(L, "game/visual_tests");
        if (lua_pcall(L, 1, 1, 0) != LUA_OK)
        {
            SDL_Log("visual tests: require(game/visual_tests) failed: %s", lua_tostring(L, -1));
            lua_close(L);
            SDL_Quit();
            return 1;
        }
        lua_getfield(L, -1, "run");
        if (!lua_isfunction(L, -1))
        {
            SDL_Log("visual tests: game/visual_tests must return table with run()");
            lua_close(L);
            SDL_Quit();
            return 1;
        }
        lua_setglobal(L, "_runVisualTests");
        lua_pop(L, 1);
        // Frame clear uses play camera color only while Playing/Paused; drive the harness as play mode.
        eng_editor_set_sim_ui_state(EngSimUiState::Playing);
    }

    s_lua_for_behaviors_reload = L;
    webview_host_set_behaviors_reload_fn(reload_behaviors_from_web_ui);

    float totalTime = 0;
    float reloadCheck = 0;
    Uint64 prev = SDL_GetTicks();
    int frame_idx = 0;

    while (!g_eng.quit)
    {
        frame_idx++;
        Uint64 now = SDL_GetTicks();
        float dt = (now - prev) / 1000.f;
        prev = now;
        if (dt > 0.05f)
            dt = 0.05f;
        totalTime += dt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev))
        {
            if (ev.type == SDL_EVENT_QUIT)
            {
                g_eng.quit = true;
            }
            else if (ev.type == SDL_EVENT_WINDOW_FOCUS_LOST)
            {
                // Key-ups can be delivered elsewhere (another app, or WKWebView); avoid stuck keys.
                SDL_ResetKeyboard();
                eng_game_keyboard_clear();
            }
            else if (ev.type == SDL_EVENT_WINDOW_RESIZED)
            {
                int pw = 0, ph = 0;
                SDL_GetWindowSizeInPixels(g_eng.window, &pw, &ph);
                webview_host_on_window_resized(pw, ph);
            }
            else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                     ev.type == SDL_EVENT_MOUSE_MOTION)
            {
                if (eng_editor_overlays_enabled())
                {
                    float lx, ly;
                    float mx = (ev.type == SDL_EVENT_MOUSE_MOTION) ? ev.motion.x : ev.button.x;
                    float my = (ev.type == SDL_EVENT_MOUSE_MOTION) ? ev.motion.y : ev.button.y;
                    bool in_game = main_window_mouse_to_lu(mx, my, &lx, &ly);
                    if (!in_game && ev.type == SDL_EVENT_MOUSE_MOTION &&
                        (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_LMASK))
                        in_game = main_window_mouse_to_lu(mx, my, &lx, &ly, true);
                    if (in_game)
                        eng_editor_input_set_mouse(lx, ly);

                    if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev.type == SDL_EVENT_MOUSE_BUTTON_UP)
                    {
                        bool down = (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN);
                        if (ev.button.button == SDL_BUTTON_LEFT)
                            eng_editor_input_button_event(0, down);
                        else if (ev.button.button == SDL_BUTTON_MIDDLE)
                            eng_editor_input_button_event(1, down);
                        else if (ev.button.button == SDL_BUTTON_RIGHT)
                            eng_editor_input_button_event(2, down);
                    }
                }
            }
            else if (ev.type == SDL_EVENT_MOUSE_WHEEL)
            {
                if (eng_editor_overlays_enabled())
                    eng_editor_input_scroll_event(ev.wheel.y);
            }
            else if (ev.type == SDL_EVENT_KEY_DOWN || ev.type == SDL_EVENT_KEY_UP)
            {
                eng_game_keyboard_handle_event(ev);

                if (ev.type == SDL_EVENT_KEY_DOWN)
                {
                    if (!ev.key.repeat && ev.key.key == SDLK_TAB)
                    {
                        bool uiVisible = webview_host_toggle_visibility();
                        if (!uiVisible)
                        {
                            // Hidden overlay should not constrain the game viewport to stale DOM rects.
                            s_ui_game_x = 0;
                            s_ui_game_y = 0;
                            s_ui_game_w = 0;
                            s_ui_game_h = 0;
                            s_ui_space_w = 0;
                            s_ui_space_h = 0;
                        }
                        continue;
                    }
                    const char *keyName = nullptr;
                    switch (ev.key.key)
                    {
                    case SDLK_LEFT:
                        keyName = "left";
                        break;
                    case SDLK_RIGHT:
                        keyName = "right";
                        break;
                    case SDLK_UP:
                        keyName = "up";
                        break;
                    case SDLK_DOWN:
                        keyName = "down";
                        break;
                    case SDLK_SPACE:
                        keyName = "space";
                        break;
                    case SDLK_A:
                        keyName = "a";
                        break;
                    case SDLK_D:
                        keyName = "d";
                        break;
                    case SDLK_W:
                        keyName = "w";
                        break;
                    case SDLK_R:
                        keyName = "r";
                        break;
                    case SDLK_ESCAPE:
                        keyName = "escape";
                        break;
                    default:
                        break;
                    }
                    if (keyName)
                    {
                        if (std::strcmp(keyName, "escape") == 0)
                        {
                            g_eng.quit = true;
                        }
                        else
                        {
                            eng_scene_keydown_lua_scripts(L, g_scene, keyName);
                        }
                    }
                }
            }
        }

        webview_host_sync_sdl_keyboard_state();
        eng_game_keyboard_sync();

        if (s_script_reload_requested.exchange(false, std::memory_order_acq_rel))
        {
            lua_State *newL = create_lua_vm_only();
            if (newL)
            {
                lua_close(L);
                L = newL;
                eng_lua_bind_main_vm(L);
                s_lua_for_behaviors_reload = L;
                s_script_paused.store(false, std::memory_order_relaxed);
                if (s_have_scene_snapshot_before_play)
                {
                    g_scene = s_scene_snapshot_before_play;
                    g_scene.resetScriptStartedFlags();
                    s_have_scene_snapshot_before_play = false;
                    // Snapshot may carry registry indices from the closed VM — invalidate before dispatch.
                    eng_on_lua_vm_replaced(g_scene);
                }
                else
                {
                    // `load_default_scene` calls `clear()` which would lua_unref on the new VM if refs
                    // were still set from the old VM.
                    eng_on_lua_vm_replaced(g_scene);
                    load_default_scene_file_into_g_scene();
                }
                SDL_Log("Engine restarted (editor)");
            }
        }

        uint32_t startPacked = s_pending_start_sim.exchange(0, std::memory_order_acq_rel);
        if (startPacked & kStartSimPendingBit)
        {
            s_script_paused.store(false, std::memory_order_relaxed);
            if (startPacked & kStartSimSnapshotBit)
            {
                s_scene_snapshot_before_play = g_scene;
                s_have_scene_snapshot_before_play = true;
            }
            eng_scene_hudplay_lua_scripts(L, g_scene);
        }

        webview_host_poll_dom_layout();

        // Design-resolution basis with web layout; when the overlay is hidden (or absent), use the
        // live window client size so the game fills the resized window in Luau coordinates.
        int layout_w = SCREEN_W, layout_h = SCREEN_H;
        if (!webview_host_web_overlay_visible())
            SDL_GetWindowSize(g_eng.window, &layout_w, &layout_h);

        int lu_w = layout_w, lu_h = layout_h;
        webview_apply_game_viewport(g_eng.renderer, g_eng.window,
                                    layout_w, layout_h,
                                    s_ui_game_x, s_ui_game_y, s_ui_game_w, s_ui_game_h,
                                    s_ui_space_w, s_ui_space_h, s_native_game_rect_pct,
                                    &lu_w, &lu_h);
        eng_sync_lua_screen_size(L, lu_w, lu_h);

        if (!s_script_paused.load(std::memory_order_relaxed))
        {
            eng_scene_update_lua_scripts(L, g_scene, dt);
        }
        eng_editor_input_sync_frame_edges();
        eng_scene_update_editor_behaviors(L, g_scene, dt);
        eng_editor_input_end_frame();
        editor_bridge_publish_scene_snapshot(g_scene);

        {
            auto &cam = eng_camera_state();
            const EngSimUiState sim = eng_editor_sim_ui_state();
            const bool playClear = (sim == EngSimUiState::Playing || sim == EngSimUiState::Paused);
            uint8_t cr = playClear ? cam.playBgR : cam.editorBgR;
            uint8_t cg = playClear ? cam.playBgG : cam.editorBgG;
            uint8_t cb = playClear ? cam.playBgB : cam.editorBgB;
            SDL_SetRenderDrawColor(g_eng.renderer, cr, cg, cb, 255);
            SDL_FRect bg = {0, 0, (float)lu_w, (float)lu_h};
            SDL_RenderFillRect(g_eng.renderer, &bg);
        }
        eng_scene_draw_lua_scripts(L, g_scene, totalTime);
        eng_scene_draw_editor_overlays(L, g_scene, totalTime);

        if (s_run_visual_tests && frame_idx == s_visual_test_after_frames)
        {
            if (!eng_run_visual_tests_lua(L))
            {
                SDL_Log("visual tests: FAILED");
                lua_close(L);
                g_eng.audio.shutdown();
                webview_host_shutdown();
                SDL_DestroyRenderer(g_eng.renderer);
                SDL_DestroyWindow(g_eng.window);
                SDL_Quit();
                return 1;
            }
            SDL_Log("visual tests: PASSED");
            g_eng.quit = true;
        }

        SDL_RenderPresent(g_eng.renderer);

#if defined(__APPLE__)
        if (s_capture_path && s_capture_after_frames > 0 && frame_idx == s_capture_after_frames)
        {
            SDL_Delay(800);
            if (!webview_host_capture_composite_png(g_eng.window, s_capture_path))
                SDL_Log("capture: failed (path=%s)", s_capture_path);
            else
                SDL_Log("capture: screencapture wrote %s", s_capture_path);
            g_eng.quit = true;
        }
#endif
    }

    lua_close(L);
    g_eng.audio.shutdown();
    webview_host_shutdown();
    SDL_DestroyRenderer(g_eng.renderer);
    SDL_DestroyWindow(g_eng.window);
    SDL_Quit();
    return 0;
}
