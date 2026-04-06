#include "immediate_draw.hpp"

#include <cmath>
#include <string>

// ── Matrix stack ─────────────────────────────────────────────────────────────

static std::vector<Affine2D> s_matrix_stack = {Affine2D::identity()};

void eng_draw_push_matrix(const Affine2D &m)
{
    s_matrix_stack.push_back(m);
}

void eng_draw_pop_matrix()
{
    if (s_matrix_stack.size() > 1)
        s_matrix_stack.pop_back();
}

void eng_draw_set_matrix(const Affine2D &m)
{
    s_matrix_stack.back() = m;
}

const Affine2D &eng_draw_current_matrix()
{
    return s_matrix_stack.back();
}

static inline void matLine(SDL_Renderer *r, const Affine2D &m,
                           float x1, float y1, float x2, float y2)
{
    Vec2 a = m.transformPoint(x1, y1);
    Vec2 b = m.transformPoint(x2, y2);
    SDL_RenderLine(r, a.x, a.y, b.x, b.y);
}

// ── Primitives ───────────────────────────────────────────────────────────────

void eng_immediate_draw_poly(SDL_Renderer *r,
                             const std::vector<Vec2> &pts,
                             Vec2 pos,
                             float angleDeg,
                             SDL_Color col)
{
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    const Affine2D &m = eng_draw_current_matrix();
    float a = angleDeg * ENG_DEG2RAD;
    float ca = cosf(a), sa = sinf(a);
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i)
    {
        Vec2 p0 = pts[i], p1 = pts[(i + 1) % n];
        float lx0 = p0.x * ca - p0.y * sa + pos.x;
        float ly0 = p0.x * sa + p0.y * ca + pos.y;
        float lx1 = p1.x * ca - p1.y * sa + pos.x;
        float ly1 = p1.x * sa + p1.y * ca + pos.y;
        matLine(r, m, lx0, ly0, lx1, ly1);
    }
}

void eng_immediate_draw_circle(SDL_Renderer *r, Vec2 c, float radius, SDL_Color col)
{
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    const Affine2D &m = eng_draw_current_matrix();
    Vec2 wc = m.transformPoint(c.x, c.y);
    float sr = radius * m.uniformScale();
    int segs = 20;
    for (int i = 0; i < segs; ++i)
    {
        float a0 = (i / (float)segs) * 2 * PI;
        float a1 = ((i + 1) / (float)segs) * 2 * PI;
        SDL_RenderLine(r,
                       wc.x + cosf(a0) * sr, wc.y + sinf(a0) * sr,
                       wc.x + cosf(a1) * sr, wc.y + sinf(a1) * sr);
    }
}

void eng_immediate_draw_char(SDL_Renderer *r, char ch, float x, float y, float scale, SDL_Color col)
{
    static const uint8_t seg7[] = {
        0b1110111,
        0b0100100,
        0b1011101,
        0b1101101,
        0b0101110,
        0b1101011,
        0b1111011,
        0b0100101,
        0b1111111,
        0b1101111,
    };
    if (ch < '0' || ch > '9')
        return;
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    const Affine2D &m = eng_draw_current_matrix();
    uint8_t segs = seg7[ch - '0'];
    float w = 6 * scale, h = 10 * scale, h2 = 5 * scale;
    if (segs & (1 << 0))
        matLine(r, m, x, y, x + w, y);
    if (segs & (1 << 1))
        matLine(r, m, x, y, x, y + h2);
    if (segs & (1 << 2))
        matLine(r, m, x + w, y, x + w, y + h2);
    if (segs & (1 << 3))
        matLine(r, m, x, y + h2, x + w, y + h2);
    if (segs & (1 << 4))
        matLine(r, m, x, y + h2, x, y + h);
    if (segs & (1 << 5))
        matLine(r, m, x + w, y + h2, x + w, y + h);
    if (segs & (1 << 6))
        matLine(r, m, x, y + h, x + w, y + h);
}

void eng_immediate_draw_number(SDL_Renderer *r, int n, float x, float y, float scale, SDL_Color col)
{
    std::string s = std::to_string(n);
    for (size_t i = 0; i < s.size(); ++i)
        eng_immediate_draw_char(r, s[i], x + i * 8 * scale, y, scale, col);
}
