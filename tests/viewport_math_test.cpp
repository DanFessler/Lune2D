// Unit tests for DOM layout → SDL viewport math (no SDL / WebKit).

#include "viewport_math.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#define TEST_ASSERT(cond, msg)                                                                    \
    do {                                                                                          \
        if (!(cond)) {                                                                            \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg);                    \
            return 1;                                                                             \
        }                                                                                         \
    } while (0)

static int test_repro_identical_screenshots_sdl3_viewport_multiplies_by_scale() {
    // Clue: composite wrong the *same way* every capture — not flaky JS/denominator. SDL3 applies
    // both RenderViewport and RenderScale: pixel clip width ≈ ceil(viewport.w * current_scale.x).
    // Passing lay.pw (already in output pixels) then SetRenderScale(lay.scale_x) applies scale twice.
    // Inset uses Luau extent = game_w (756); render scale = pw/756 → 2× on golden Retina.
    const float scale_x = 1512.f / 756.f;
    const int   pixel_w   = 1512;
    int         effective_buggy = (int)std::ceil((double)pixel_w * (double)scale_x);
    TEST_ASSERT(effective_buggy > pixel_w + 100, "double-apply must widen clip vs intended pixels");

    int pre_w = (int)std::lround((double)pixel_w / (double)scale_x);
    TEST_ASSERT(pre_w == 756, "pre-scale viewport width matches #game-surface layout width");
    TEST_ASSERT(std::abs((double)pre_w * (double)scale_x - (double)pixel_w) < 0.15,
        "round-trip to pixel width");
    return 0;
}

static int test_hypothesis_zero_game_rect_skips_inset_math() {
    // If the bridge never delivers a DOM rect (game_w/game_h stay 0), the inset branch must NOT
    // run — otherwise we'd be "fixing" viewport math while the real issue is stale zero dims.
    ViewportLayoutInput in{};
    in.game_w    = 0;
    in.game_h    = 0;
    in.wk_w      = 900;
    in.wk_h      = 700;
    in.ui_space_w = 900;
    in.ui_space_h = 700;
    in.out_w     = 1800;
    in.out_h     = 1400;
    in.logical_w = 900;
    in.logical_h = 700;

    ViewportLayoutResult r = compute_viewport_layout(in);
    TEST_ASSERT(!r.uses_dom_inset, "game_w=0 must not use DOM inset");
    TEST_ASSERT(r.pw == 1800 && r.ph == 1400, "full output when no DOM rect");
    TEST_ASSERT(r.px == 0 && r.py == 0, "origin 0 when full output");
    TEST_ASSERT(r.lu_w == 900 && r.lu_h == 700, "full frame uses design logical size");
    return 0;
}

static int test_hypothesis_nonzero_dom_rect_triggers_inset() {
    ViewportLayoutInput in{};
    in.game_x    = 72;
    in.game_y    = 84;
    in.game_w    = 756;
    in.game_h    = 532;
    in.wk_w      = 900;
    in.wk_h      = 700;
    in.ui_space_w = 900;
    in.ui_space_h = 700;
    in.out_w     = 1800;
    in.out_h     = 1400;
    in.logical_w = 900;
    in.logical_h = 700;

    ViewportLayoutResult r = compute_viewport_layout(in);
    TEST_ASSERT(r.uses_dom_inset, "positive DOM size must enable inset");
    TEST_ASSERT(r.pw < in.out_w && r.ph < in.out_h, "inset smaller than full target");
    return 0;
}

static int test_golden_retina_mapping_layout_points_to_output_pixels() {
    // Layout 900×700 (points), backing store 1800×1400, DOM #game-surface 8%/12%/84%/76%.
    ViewportLayoutInput in{};
    in.game_x     = 72;
    in.game_y     = 84;
    in.game_w     = 756;
    in.game_h     = 532;
    in.ui_space_w = 900;
    in.ui_space_h = 700;
    in.wk_w       = 900;
    in.wk_h       = 700;
    in.out_w      = 1800;
    in.out_h      = 1400;
    in.logical_w  = 900;
    in.logical_h  = 700;

    ViewportLayoutResult r = compute_viewport_layout(in);

    TEST_ASSERT(r.px == 144, "px");
    TEST_ASSERT(r.py == 168, "py");
    TEST_ASSERT(r.pw == 1512, "pw");
    TEST_ASSERT(r.ph == 1064, "ph");
    TEST_ASSERT(r.lu_w == 756 && r.lu_h == 532, "Luau extent matches #game-surface");
    TEST_ASSERT(std::fabs(r.scale_x - 2.f) < 1e-3f, "scale_x integer 2×");
    TEST_ASSERT(std::fabs(r.scale_y - 2.f) < 1e-3f, "scale_y integer 2×");
    return 0;
}

static int test_repro_composite_misalign_wk_ui_out_all_equal_backing_while_dom_is_layout_css() {
    // Screenshot repro (SDL region ~½ width of pink #game-surface): WKWebView.bounds and JS uiw/uih
    // can all match SDL render-output PIXELS (1800×1400) while getBoundingClientRect() is still in
    // layout/CSS space for the 900×700 design (same 72,84,756,532 as index.html). Then denom=1800
    // → sx=1 → pw=756. This test fails until we infer layout basis (900×700) for that case.
    ViewportLayoutInput in{};
    in.game_x     = 72;
    in.game_y     = 84;
    in.game_w     = 756;
    in.game_h     = 532;
    in.ui_space_w = 1800;
    in.ui_space_h = 1400;
    in.wk_w       = 1800;
    in.wk_h       = 1400;
    in.out_w      = 1800;
    in.out_h      = 1400;
    in.logical_w  = 900;
    in.logical_h  = 700;

    ViewportLayoutResult r = compute_viewport_layout(in);
    TEST_ASSERT(r.uses_dom_inset, "inset");
    TEST_ASSERT(r.pw == 1512 && r.ph == 1064, "must not use backing as layout denom (1512 not 756)");
    TEST_ASSERT(r.px == 144 && r.py == 168, "origin");
    return 0;
}

static int test_repro_ui_space_matches_backing_store_but_rect_is_layout_css() {
    // REAL REPRO (must FAIL before fix): getBoundingClientRect() is in layout/CSS px (756 wide on a
    // 900-logical view), but JS sometimes posts uiw/uih equal to the backing-store size (1800×1400).
    // Using 1800 as the layout denominator makes sx=1 → pw=756 instead of pw=1512 → tiny viewport.
    // Expected: still match golden Retina mapping 144,168,1512,1064.
    ViewportLayoutInput in{};
    in.game_x     = 72;
    in.game_y     = 84;
    in.game_w     = 756;
    in.game_h     = 532;
    in.ui_space_w = 1800; // wrong from page (backing dimensions), WK layout is 900
    in.ui_space_h = 1400;
    in.wk_w       = 900;
    in.wk_h       = 700;
    in.out_w      = 1800;
    in.out_h      = 1400;
    in.logical_w  = 900;
    in.logical_h  = 700;

    ViewportLayoutResult r = compute_viewport_layout(in);
    TEST_ASSERT(r.uses_dom_inset, "inset");
    TEST_ASSERT(r.px == 144 && r.py == 168, "origin");
    TEST_ASSERT(r.pw == 1512 && r.ph == 1064, "size (1512 not 756)");
    return 0;
}

static int test_native_pct_matches_css_fractions() {
    ViewportLayoutInput in{};
    in.game_w          = 0;
    in.game_h          = 0;
    in.ui_space_w      = 900;
    in.ui_space_h      = 700;
    in.wk_w            = 900;
    in.wk_h            = 700;
    in.out_w           = 1800;
    in.out_h           = 1400;
    in.logical_w       = 900;
    in.logical_h       = 700;
    in.native_pct_rect = true;

    ViewportLayoutResult r = compute_viewport_layout(in);
    TEST_ASSERT(r.uses_dom_inset, "native pct should produce inset");
    TEST_ASSERT(r.px == 144 && r.py == 168 && r.pw == 1512 && r.ph == 1064, "native pct golden");
    TEST_ASSERT(r.lu_w == 756 && r.lu_h == 532, "native pct lu extent");
    return 0;
}

int main() {
    using T = int (*)();
    T tests[] = {
        test_repro_identical_screenshots_sdl3_viewport_multiplies_by_scale,
        test_hypothesis_zero_game_rect_skips_inset_math,
        test_hypothesis_nonzero_dom_rect_triggers_inset,
        test_golden_retina_mapping_layout_points_to_output_pixels,
        test_repro_composite_misalign_wk_ui_out_all_equal_backing_while_dom_is_layout_css,
        test_repro_ui_space_matches_backing_store_but_rect_is_layout_css,
        test_native_pct_matches_css_fractions,
    };
    for (T t : tests) {
        int e = t();
        if (e != 0)
            return e;
    }
    std::printf("viewport_math: all tests passed\n");
    return 0;
}
