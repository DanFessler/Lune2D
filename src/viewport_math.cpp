#include "viewport_math.hpp"

#include <cmath>
#include <cstdlib>

namespace
{

    constexpr double kGameLeftFrac = 0.08;
    constexpr double kGameTopFrac = 0.12;
    constexpr double kGameWidthFrac = 0.84;
    constexpr double kGameHeightFrac = 0.76;

} // namespace

ViewportLayoutResult compute_viewport_layout(const ViewportLayoutInput &in)
{
    ViewportLayoutResult r{};
    r.pw = in.out_w;
    r.ph = in.out_h;

    int denom_w = in.wk_w;
    int denom_h = in.wk_h;
    if (in.ui_space_w > 0 && in.ui_space_h > 0)
    {
        // JS sometimes reports backing-store dimensions (== out_w/out_h) while getBoundingClientRect
        // is still in layout/CSS space relative to the webview frame (wk_w × wk_h in points). Using
        // the pixel size as the layout denominator collapses the mapped viewport by the scale.
        bool ui_matches_output = (in.ui_space_w == in.out_w && in.ui_space_h == in.out_h);
        bool wk_is_layout_basis = (in.wk_w > 0 && in.wk_h > 0 &&
                                   (in.ui_space_w != in.wk_w || in.ui_space_h != in.wk_h));
        if (!(ui_matches_output && wk_is_layout_basis))
        {
            denom_w = in.ui_space_w;
            denom_h = in.ui_space_h;
        }
    }

    // If WK bounds, JS ui space, and drawable output all agree in backing PIXELS—but the posted DOM
    // rect still matches our HTML template in LOGICAL game coordinates—we must not use out_w as the
    // %-basis (would set sx=1 and halve the viewport vs the dashed border on Retina).
    {
        bool all_match_backing = in.wk_w == in.out_w && in.wk_h == in.out_h &&
                                 in.ui_space_w == in.out_w && in.ui_space_h == in.out_h;
        auto near_frac = [](int v, double frac, int L) -> bool
        {
            return std::abs(v - (int)std::lround(frac * (double)L)) <= 1;
        };
        bool dom_matches_css_template =
            near_frac(in.game_x, kGameLeftFrac, in.logical_w) &&
            near_frac(in.game_y, kGameTopFrac, in.logical_h) &&
            near_frac(in.game_w, kGameWidthFrac, in.logical_w) &&
            near_frac(in.game_h, kGameHeightFrac, in.logical_h);
        if (all_match_backing && dom_matches_css_template && in.out_w > in.logical_w &&
            in.logical_w > 0 && in.logical_h > 0 && in.game_w > 0 && in.game_h > 0)
        {
            denom_w = in.logical_w;
            denom_h = in.logical_h;
        }
    }

    int game_x = in.game_x;
    int game_y = in.game_y;
    int game_w = in.game_w;
    int game_h = in.game_h;

    if (in.native_pct_rect && denom_w > 0 && denom_h > 0)
    {
        game_x = (int)std::lround(kGameLeftFrac * denom_w);
        game_y = (int)std::lround(kGameTopFrac * denom_h);
        game_w = (int)std::lround(kGameWidthFrac * denom_w);
        game_h = (int)std::lround(kGameHeightFrac * denom_h);
    }

    if (denom_w > 0 && denom_h > 0 && in.out_w > 0 && in.out_h > 0 && game_w > 0 && game_h > 0)
    {
        float sx = in.out_w / (float)denom_w;
        float sy = in.out_h / (float)denom_h;
        r.px = (int)std::lround((double)game_x * sx);
        r.py = (int)std::lround((double)game_y * sy);
        r.pw = (int)std::lround((double)game_w * sx);
        r.ph = (int)std::lround((double)game_h * sy);
        r.uses_dom_inset = true;
        // Map Luau 0..game_w to viewport pixels (integer scale on typical 2× Retina vs 900×700 basis).
        r.scale_x = r.pw / (float)game_w;
        r.scale_y = r.ph / (float)game_h;
        r.lu_w = game_w;
        r.lu_h = game_h;
    }
    else
    {
        r.scale_x = r.pw / (float)in.logical_w;
        r.scale_y = r.ph / (float)in.logical_h;
        r.lu_w = in.logical_w;
        r.lu_h = in.logical_h;
    }

    return r;
}
