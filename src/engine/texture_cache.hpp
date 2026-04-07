#pragma once

#include <SDL3/SDL.h>

#include <string>

/// Load or return cached `SDL_Texture` for a project-relative image path (UTF-8).
/// @param relPath Forward slashes only; must not contain "..".
SDL_Texture* eng_texture_get(SDL_Renderer* renderer, const std::string& relPath);

/// Destroy all cached textures (e.g. behavior hot reload).
void eng_texture_cache_clear();
