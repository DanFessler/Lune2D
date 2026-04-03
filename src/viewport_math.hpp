#pragma once

// Pure layout: DOM / layout points → SDL render-output pixels + scale (no SDL calls).

struct ViewportLayoutInput {
    int  game_x = 0, game_y = 0, game_w = 0, game_h = 0;
    int  ui_space_w = 0, ui_space_h = 0;
    int  wk_w = 0, wk_h = 0;
    int  out_w = 0, out_h = 0;
    /// Layout viewport used for CSS %-template matching (e.g. 900×700 window basis), not Luau extent.
    int  logical_w = 900, logical_h = 700;
    bool native_pct_rect = false;
};

struct ViewportLayoutResult {
    int   px = 0, py = 0, pw = 0, ph = 0;
    float scale_x = 1.f, scale_y = 1.f;
    /// Luau / draw coordinate extent: game surface in layout px when inset, else logical_w/h.
    int   lu_w = 900, lu_h = 700;
    /// True iff non-zero DOM game rect was mapped to an inset (not full output).
    bool uses_dom_inset = false;
};

ViewportLayoutResult compute_viewport_layout(const ViewportLayoutInput& in);
