#include "game_keyboard.hpp"

#include "engine/engine.hpp"

#include <cstring>

#ifdef __APPLE__
#include <CoreGraphics/CGEventSource.h>
#endif

static bool     s_keys[SDL_SCANCODE_COUNT];
static uint16_t s_native[SDL_SCANCODE_COUNT];   // ev.key.raw → macOS CGKeyCode

static constexpr uint16_t kUnmapped = 0xFFFF;

void eng_game_keyboard_init(Engine* eng)
{
    std::memset(s_keys, 0, sizeof(s_keys));
    std::memset(s_native, 0xFF, sizeof(s_native));
    if (eng)
        eng->keys = s_keys;
}

void eng_game_keyboard_handle_event(const SDL_Event& ev)
{
    if (ev.type == SDL_EVENT_KEY_DOWN)
    {
        SDL_Scancode sc = ev.key.scancode;
        if (sc > SDL_SCANCODE_UNKNOWN && sc < SDL_SCANCODE_COUNT)
        {
            s_keys[sc] = true;
            if (ev.key.raw != 0)
                s_native[sc] = ev.key.raw;
        }
    }
    else if (ev.type == SDL_EVENT_KEY_UP)
    {
        SDL_Scancode sc = ev.key.scancode;
        if (sc > SDL_SCANCODE_UNKNOWN && sc < SDL_SCANCODE_COUNT)
            s_keys[sc] = false;
    }
}

void eng_game_keyboard_sync()
{
#ifdef __APPLE__
    // Query actual hardware key state via Quartz — the only source of truth that
    // can't be desynchronized by missed NSEvents, WKWebView stealing focus, SDL
    // internal bookkeeping, or multi-key ghosting at the event layer.
    for (int i = 1; i < SDL_SCANCODE_COUNT; ++i)
    {
        if (!s_keys[i])
            continue;
        if (s_native[i] == kUnmapped)
            continue;
        if (!CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState,
                                   (CGKeyCode)s_native[i]))
            s_keys[i] = false;
    }
#else
    const bool* sdl = SDL_GetKeyboardState(nullptr);
    if (!sdl)
        return;
    for (int i = 1; i < SDL_SCANCODE_COUNT; ++i)
        if (s_keys[i] && !sdl[i])
            s_keys[i] = false;
#endif
}

void eng_game_keyboard_clear()
{
    std::memset(s_keys, 0, sizeof(s_keys));
}
