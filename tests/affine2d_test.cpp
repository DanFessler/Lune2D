// TDD tests for Affine2D::inverse() and fromScale() (Phase 0a).

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

static constexpr float EPS = 1e-4f;

static bool affine_near(const Affine2D &l, const Affine2D &r, float eps = EPS)
{
    return std::fabs(l.a - r.a) < eps && std::fabs(l.b - r.b) < eps &&
           std::fabs(l.tx - r.tx) < eps && std::fabs(l.c - r.c) < eps &&
           std::fabs(l.d - r.d) < eps && std::fabs(l.ty - r.ty) < eps;
}

static int test_identity_inverse()
{
    Affine2D inv = Affine2D::identity().inverse();
    TEST_ASSERT(affine_near(inv, Affine2D::identity()), "identity.inverse() == identity");
    return 0;
}

static int test_translation_inverse()
{
    Affine2D inv = Affine2D::fromTranslation(10.f, -5.f).inverse();
    Affine2D expected = Affine2D::fromTranslation(-10.f, 5.f);
    TEST_ASSERT(affine_near(inv, expected), "translation inverse negates offsets");
    return 0;
}

static int test_scale_inverse()
{
    Affine2D inv = Affine2D::fromScale(2.f, 4.f).inverse();
    Affine2D expected = Affine2D::fromScale(0.5f, 0.25f);
    TEST_ASSERT(affine_near(inv, expected), "scale inverse reciprocals");
    return 0;
}

static int test_rotation_inverse()
{
    Affine2D fwd = Affine2D::fromTRS(0, 0, 45, 1, 1);
    Affine2D inv = fwd.inverse();
    Affine2D expected = Affine2D::fromTRS(0, 0, -45, 1, 1);
    TEST_ASSERT(affine_near(inv, expected), "rotation inverse negates angle");
    return 0;
}

static int test_trs_round_trip()
{
    Affine2D m = Affine2D::fromTRS(100, -50, 30, 2, 3);
    Affine2D product = m.multiply(m.inverse());
    TEST_ASSERT(affine_near(product, Affine2D::identity()),
                "m * m.inverse() == identity for arbitrary TRS");
    return 0;
}

static int test_transform_point_round_trip()
{
    Affine2D m = Affine2D::fromTRS(100, -50, 30, 2, 3);
    float px = 7.f, py = -13.f;
    Vec2 fwd = m.transformPoint(px, py);
    Vec2 back = m.inverse().transformPoint(fwd.x, fwd.y);
    TEST_ASSERT(std::fabs(back.x - px) < EPS && std::fabs(back.y - py) < EPS,
                "inverse.transformPoint round-trips");
    return 0;
}

static int test_from_scale_basic()
{
    Affine2D s = Affine2D::fromScale(3.f, 5.f);
    Vec2 p = s.transformPoint(2.f, 4.f);
    TEST_ASSERT(std::fabs(p.x - 6.f) < EPS && std::fabs(p.y - 20.f) < EPS,
                "fromScale(3,5) * (2,4) == (6,20)");
    return 0;
}

int main()
{
    int failures = 0;

#define RUN(fn) do { if (fn()) { failures++; std::fprintf(stderr, "  in " #fn "\n"); } } while(0)

    RUN(test_identity_inverse);
    RUN(test_translation_inverse);
    RUN(test_scale_inverse);
    RUN(test_rotation_inverse);
    RUN(test_trs_round_trip);
    RUN(test_transform_point_round_trip);
    RUN(test_from_scale_basic);

#undef RUN

    if (failures == 0)
        std::printf("All affine2d tests passed.\n");
    else
        std::fprintf(stderr, "%d test(s) failed.\n", failures);
    return failures;
}
