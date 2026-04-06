#include "lua_vm_lifecycle.hpp"

#include "scene.hpp"

#include <SDL3/SDL.h>

static uint32_t s_lua_vm_generation = 0;

uint32_t eng_lua_vm_generation() {
    return s_lua_vm_generation;
}

void eng_on_lua_vm_replaced(Scene& scene) {
    ++s_lua_vm_generation;
    scene.invalidateAllBehaviorLuaRefs();
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                 "Luau behavior VM replaced: generation=%u (invalidated script refs)",
                 (unsigned)s_lua_vm_generation);
}
