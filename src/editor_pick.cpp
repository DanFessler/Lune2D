#include "editor_pick.hpp"

#include <limits>

#include "scene.hpp"

std::uint32_t eng_editor_pick_entity_at_lu(Scene &scene, float lu_x, float lu_y, float hit_r)
{
    auto wm = scene.computeWorldMatrices();
    std::uint32_t best = 0;
    int best_draw = std::numeric_limits<int>::min();
    float best_d2 = 0.f;
    const float hr2 = hit_r * hit_r;

    for (const Entity *ep : scene.entitiesSortedById())
    {
        const Entity &e = *ep;
        if (!e.active)
            continue;
        auto it = wm.find(e.id);
        if (it == wm.end())
            continue;
        float wx = it->second.tx;
        float wy = it->second.ty;
        float dx = lu_x - wx;
        float dy = lu_y - wy;
        float d2 = dx * dx + dy * dy;
        if (d2 > hr2)
            continue;

        if (e.drawOrder > best_draw ||
            (e.drawOrder == best_draw && (best == 0 || d2 < best_d2)))
        {
            best_draw = e.drawOrder;
            best_d2 = d2;
            best = e.id;
        }
    }
    return best;
}
