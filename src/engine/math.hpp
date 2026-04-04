#pragma once

#include <cmath>
#include <cstdlib>

struct Vec2 {
    float x = 0, y = 0;
};

/// 2D affine transform matrix (6 floats):
///   | a  b  tx |
///   | c  d  ty |
///   | 0  0   1 |
struct Affine2D {
    float a = 1, b = 0, tx = 0;
    float c = 0, d = 1, ty = 0;

    static Affine2D identity() { return {}; }

    static Affine2D fromTRS(float x, float y, float angleDeg, float sx, float sy) {
        float rad = angleDeg * (3.14159265f / 180.f);
        float ca  = cosf(rad), sa = sinf(rad);
        return { sx * ca, -sy * sa, x,
                 sx * sa,  sy * ca, y };
    }

    static Affine2D fromTranslation(float x, float y) {
        return { 1, 0, x, 0, 1, y };
    }

    Affine2D multiply(const Affine2D& r) const {
        return { a * r.a + b * r.c,       a * r.b + b * r.d,       a * r.tx + b * r.ty + tx,
                 c * r.a + d * r.c,       c * r.b + d * r.d,       c * r.tx + d * r.ty + ty };
    }

    Vec2 transformPoint(float px, float py) const {
        return { a * px + b * py + tx,
                 c * px + d * py + ty };
    }

    float uniformScale() const {
        return sqrtf(a * a + c * c);
    }
};

inline float eng_randf(float lo, float hi) {
    return lo + (hi - lo) * (rand() / (float)RAND_MAX);
}
