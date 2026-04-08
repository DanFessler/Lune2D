// TDD tests for camera view matrix computation and coordinate conversion (Phase 0b).

#include "engine/camera.hpp"
#include "engine/math.hpp"

#include <cmath>
#include <cstdio>

#define TEST_ASSERT(cond, msg)                                                 \
    do {                                                                       \
        if (!(cond)) {                                                         \
            std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
            return 1;                                                          \
        }                                                                      \
    } while (0)

static constexpr float EPS = 1e-3f;

static bool affine_near(const Affine2D &l, const Affine2D &r, float eps = EPS)
{
    return std::fabs(l.a - r.a) < eps && std::fabs(l.b - r.b) < eps &&
           std::fabs(l.tx - r.tx) < eps && std::fabs(l.c - r.c) < eps &&
           std::fabs(l.d - r.d) < eps && std::fabs(l.ty - r.ty) < eps;
}

static int test_backward_compat_identity()
{
    const int W = 900, H = 700;
    Affine2D v = eng_compute_view_matrix(W / 2.f, H / 2.f, 0.f, (float)H, W, H);
    TEST_ASSERT(affine_near(v, Affine2D::identity()),
                "default camera (center, vfov=screenH) produces identity");
    return 0;
}

static int test_camera_center_maps_to_screen_center()
{
    const int W = 800, H = 600;
    float camX = 50, camY = 30, vfov = 100;
    Affine2D v = eng_compute_view_matrix(camX, camY, 0.f, vfov, W, H);
    Vec2 p = v.transformPoint(camX, camY);
    TEST_ASSERT(std::fabs(p.x - W / 2.f) < EPS && std::fabs(p.y - H / 2.f) < EPS,
                "camera center maps to screen center");
    return 0;
}

static int test_vertical_extent()
{
    const int W = 800, H = 600;
    float camX = 50, camY = 30, vfov = 100;
    Affine2D v = eng_compute_view_matrix(camX, camY, 0.f, vfov, W, H);
    Vec2 bottom = v.transformPoint(camX, camY + vfov / 2.f);
    TEST_ASSERT(std::fabs(bottom.x - W / 2.f) < EPS, "bottom edge x == screen center x");
    TEST_ASSERT(std::fabs(bottom.y - (float)H) < EPS, "camY + vfov/2 maps to screen bottom");
    return 0;
}

static int test_ppu_scaling()
{
    const int W = 900, H = 700;
    float ppu = 16.f;
    float camX = W / 2.f / ppu, camY = H / 2.f / ppu;
    float vfov = H / ppu;
    Affine2D v = eng_compute_view_matrix(camX, camY, 0.f, vfov, W, H);
    // scale = H / vfov = H / (H/ppu) = ppu. View matrix = fromScale(ppu, ppu).
    Affine2D expected = Affine2D::fromScale(ppu, ppu);
    TEST_ASSERT(affine_near(v, expected),
                "PPU-scaled default camera produces scale(ppu)");
    return 0;
}

static int test_zoom()
{
    const int W = 800, H = 600;
    float camX = 0, camY = 0;
    float vfov = (float)H / 2.f;
    Affine2D v = eng_compute_view_matrix(camX, camY, 0.f, vfov, W, H);
    // scale = H / vfov = 2.0
    Vec2 p = v.transformPoint(camX + 1, camY);
    float expected_x = W / 2.f + 2.f;
    TEST_ASSERT(std::fabs(p.x - expected_x) < EPS,
                "vfov=H/2 doubles visual size (1 world unit = 2 screen px from center)");
    return 0;
}

static int test_rotation()
{
    const int S = 600;
    float camX = 0, camY = 0;
    Affine2D v = eng_compute_view_matrix(camX, camY, 90.f, (float)S, S, S);
    // scale = 1.0, rotate(-90°): world +X (1,0) maps to screen -Y offset from center
    Vec2 p = v.transformPoint(1, 0);
    float expected_x = S / 2.f;
    float expected_y = S / 2.f - 1.f;
    TEST_ASSERT(std::fabs(p.x - expected_x) < EPS && std::fabs(p.y - expected_y) < EPS,
                "90-deg camera rotation: world +X maps to screen -Y");
    return 0;
}

static int test_screen_to_world_round_trip()
{
    const int W = 800, H = 600;
    Affine2D v = eng_compute_view_matrix(50, 30, 25.f, 100.f, W, H);
    Affine2D inv = v.inverse();

    float sx = 123.f, sy = 456.f;
    Vec2 w = inv.transformPoint(sx, sy);
    Vec2 back = v.transformPoint(w.x, w.y);
    TEST_ASSERT(std::fabs(back.x - sx) < EPS && std::fabs(back.y - sy) < EPS,
                "worldToScreen(screenToWorld(s)) == s");
    return 0;
}

static int test_screen_to_world_identity()
{
    const int W = 900, H = 700;
    Affine2D v = eng_compute_view_matrix(W / 2.f, H / 2.f, 0.f, (float)H, W, H);
    Affine2D inv = v.inverse();
    Vec2 p = inv.transformPoint(100.f, 200.f);
    TEST_ASSERT(std::fabs(p.x - 100.f) < EPS && std::fabs(p.y - 200.f) < EPS,
                "identity view: screenToWorld passthrough");
    return 0;
}

int main()
{
    int failures = 0;

#define RUN(fn) do { if (fn()) { failures++; std::fprintf(stderr, "  in " #fn "\n"); } } while(0)

    RUN(test_backward_compat_identity);
    RUN(test_camera_center_maps_to_screen_center);
    RUN(test_vertical_extent);
    RUN(test_ppu_scaling);
    RUN(test_zoom);
    RUN(test_rotation);
    RUN(test_screen_to_world_round_trip);
    RUN(test_screen_to_world_identity);

#undef RUN

    if (failures == 0)
        std::printf("All camera math tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) failed.\n", failures);
    return failures;
}
