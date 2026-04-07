#include "texture_cache.hpp"

#include "engine/engine.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static std::unordered_map<std::string, SDL_Texture*> s_tex_cache;

static bool rel_path_ok(const std::string& rel)
{
    if (rel.empty())
        return false;
    if (rel[0] == '/' || rel[0] == '\\')
        return false;
    if (rel.find("..") != std::string::npos)
        return false;
    return true;
}

static std::string join_under(const std::string& root, const std::string& rel)
{
    if (root.empty())
        return rel;
    char sep = '/';
    bool rootSlash = !root.empty() && (root.back() == '/' || root.back() == '\\');
    if (rootSlash)
        return root + rel;
    return root + sep + rel;
}

void eng_texture_cache_clear()
{
    for (auto& kv : s_tex_cache)
    {
        if (kv.second)
            SDL_DestroyTexture(kv.second);
    }
    s_tex_cache.clear();
}

static SDL_Texture* load_texture_file(SDL_Renderer* renderer, const std::string& absPath)
{
    int w = 0, h = 0, comp = 0;
    stbi_uc* pixels = stbi_load(absPath.c_str(), &w, &h, &comp, 4);
    if (!pixels || w < 1 || h < 1)
    {
        if (pixels)
            stbi_image_free(pixels);
        return nullptr;
    }

    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!tex)
    {
        stbi_image_free(pixels);
        return nullptr;
    }

    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);

    if (!SDL_UpdateTexture(tex, nullptr, pixels, w * 4))
    {
        SDL_DestroyTexture(tex);
        stbi_image_free(pixels);
        return nullptr;
    }

    stbi_image_free(pixels);
    return tex;
}

SDL_Texture* eng_texture_get(SDL_Renderer* renderer, const std::string& relPath)
{
    if (!renderer || !rel_path_ok(relPath))
        return nullptr;

    auto it = s_tex_cache.find(relPath);
    if (it != s_tex_cache.end())
        return it->second;

    const std::string& root = eng_project_lua_dir();
    std::string        abs  = join_under(root, relPath);

    SDL_Texture* tex = load_texture_file(renderer, abs);
    if (!tex)
        return nullptr;

    s_tex_cache[relPath] = tex;
    return tex;
}
