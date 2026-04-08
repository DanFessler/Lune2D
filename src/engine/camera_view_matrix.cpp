#include "camera.hpp"

#include <cmath>

Affine2D eng_compute_view_matrix(float camX, float camY, float camAngle,
                                 float vfov, int screenW, int screenH)
{
    if (vfov < 1e-6f)
        vfov = (float)screenH;

    float scale = (float)screenH / vfov;
    float rad = camAngle * (3.14159265f / 180.f);
    float ca = cosf(rad), sa = sinf(rad);

    // rotate(-angle) * translate(-camX, -camY)
    Affine2D rot_trans = {ca, sa, -camX * ca - camY * sa,
                          -sa, ca, camX * sa - camY * ca};

    // scale(scale) * rot_trans
    Affine2D scaled = {rot_trans.a * scale, rot_trans.b * scale, rot_trans.tx * scale,
                       rot_trans.c * scale, rot_trans.d * scale, rot_trans.ty * scale};

    // translate(screenW/2, screenH/2) * scaled
    scaled.tx += screenW / 2.f;
    scaled.ty += screenH / 2.f;

    return scaled;
}
