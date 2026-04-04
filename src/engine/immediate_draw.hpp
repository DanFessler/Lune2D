#pragma once

#include <SDL3/SDL.h>

#include <vector>

#include "constants.hpp"
#include "math.hpp"

static constexpr float ENG_DEG2RAD = PI / 180.f;

// ── Matrix stack ─────────────────────────────────────────────────────────────

void           eng_draw_push_matrix(const Affine2D& m);
void           eng_draw_pop_matrix();
void           eng_draw_set_matrix(const Affine2D& m);
const Affine2D& eng_draw_current_matrix();

// ── Primitives (all coordinates pass through the current matrix) ─────────────

void eng_immediate_draw_poly(SDL_Renderer* r,
                             const std::vector<Vec2>& pts,
                             Vec2                     pos,
                             float                    angleDeg,
                             SDL_Color                col);

void eng_immediate_draw_circle(SDL_Renderer* r, Vec2 c, float radius, SDL_Color col);

void eng_immediate_draw_char(SDL_Renderer* r, char ch, float x, float y, float scale, SDL_Color col);

void eng_immediate_draw_number(SDL_Renderer* r, int n, float x, float y, float scale, SDL_Color col);
