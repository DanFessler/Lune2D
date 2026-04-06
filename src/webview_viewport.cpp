#include "webview_host.hpp"
#include "viewport_math.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>

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
                                 int *out_lu_w,
                                 int *out_lu_h)
{
    int wk_w = 0, wk_h = 0;
    webview_host_get_layout_basis(&wk_w, &wk_h);
    if (wk_w <= 0 || wk_h <= 0)
        SDL_GetWindowSize(window, &wk_w, &wk_h);

    int out_w = 0, out_h = 0;
    SDL_GetRenderOutputSize(renderer, &out_w, &out_h);

    if (wk_w > 0 && wk_h > 0 &&
        (std::abs(ui_space_w - wk_w) > 2 || std::abs(ui_space_h - wk_h) > 2) &&
        ui_space_w > 0 && ui_space_h > 0)
    {
        SDL_Log("viewport: JS layout %dx%d vs WK bounds %dx%d", ui_space_w, ui_space_h, wk_w, wk_h);
    }

    ViewportLayoutInput vin{};
    vin.game_x = game_x;
    vin.game_y = game_y;
    vin.game_w = game_w;
    vin.game_h = game_h;
    vin.ui_space_w = ui_space_w;
    vin.ui_space_h = ui_space_h;
    vin.wk_w = wk_w;
    vin.wk_h = wk_h;
    vin.out_w = out_w;
    vin.out_h = out_h;
    vin.logical_w = logical_w;
    vin.logical_h = logical_h;
    vin.native_pct_rect = native_pct_rect;

    ViewportLayoutResult lay = compute_viewport_layout(vin);
    if (out_lu_w)
        *out_lu_w = lay.lu_w;
    if (out_lu_h)
        *out_lu_h = lay.lu_h;

    SDL_SetRenderDrawColor(renderer, 12, 14, 22, 255);
    SDL_SetRenderViewport(renderer, nullptr);
    SDL_SetRenderScale(renderer, 1.f, 1.f);
    SDL_RenderClear(renderer);

    // SDL3: the GPU clip rectangle is pixel_viewport = ceil(viewport * current_scale) (see
    // UpdatePixelViewport in SDL_render.c). Setting a pixel-sized rect then SetRenderScale
    // multiplies again — same wrong composite every frame (matches screenshots).
    float sx = lay.scale_x > 1e-6f ? lay.scale_x : 1.f;
    float sy = lay.scale_y > 1e-6f ? lay.scale_y : 1.f;
    SDL_SetRenderScale(renderer, sx, sy);
    SDL_Rect vp = {
        (int)std::lround((double)lay.px / sx),
        (int)std::lround((double)lay.py / sy),
        (int)std::lround((double)lay.pw / sx),
        (int)std::lround((double)lay.ph / sy),
    };
    SDL_SetRenderViewport(renderer, &vp);
}

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
                                  float *out_lu_y)
{
    if (!out_lu_x || !out_lu_y)
        return false;

    int wk_w = 0, wk_h = 0;
    webview_host_get_layout_basis(&wk_w, &wk_h);
    if (wk_w <= 0 || wk_h <= 0)
        SDL_GetWindowSize(window, &wk_w, &wk_h);

    int out_w = 0, out_h = 0;
    SDL_GetRenderOutputSize(renderer, &out_w, &out_h);

    ViewportLayoutInput vin{};
    vin.game_x = game_x;
    vin.game_y = game_y;
    vin.game_w = game_w;
    vin.game_h = game_h;
    vin.ui_space_w = ui_space_w;
    vin.ui_space_h = ui_space_h;
    vin.wk_w = wk_w;
    vin.wk_h = wk_h;
    vin.out_w = out_w;
    vin.out_h = out_h;
    vin.logical_w = logical_w;
    vin.logical_h = logical_h;
    vin.native_pct_rect = native_pct_rect;

    ViewportLayoutResult lay = compute_viewport_layout(vin);

    int ww = 0, wh = 0;
    SDL_GetWindowSize(window, &ww, &wh);
    if (ww < 1)
        ww = 1;
    if (wh < 1)
        wh = 1;

    float rx = window_mouse_x * (out_w / (float)ww);
    float ry = window_mouse_y * (out_h / (float)wh);

    if (rx < lay.px || ry < lay.py || rx >= lay.px + lay.pw || ry >= lay.py + lay.ph)
        return false;

    *out_lu_x = (rx - lay.px) * (float)lay.lu_w / (float)std::max(1, lay.pw);
    *out_lu_y = (ry - lay.py) * (float)lay.lu_h / (float)std::max(1, lay.ph);
    return true;
}
