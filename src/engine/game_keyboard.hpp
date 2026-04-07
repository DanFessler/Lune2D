#pragma once

#include <SDL3/SDL.h>

struct Engine;

/// Owned keyboard state for game `input.down`.
///
/// Replaces raw SDL_GetKeyboardState with a buffer that is:
///   1. SET by SDL KEY_DOWN events (responsive, no polling lag)
///   2. CLEARED by SDL KEY_UP events
///   3. RECONCILED every frame by `sync()`:
///        - On macOS: queries the OS hardware key bitmap via CGEventSourceKeyState
///          (bypasses SDL entirely — catches lost KEY_UPs from ghosting, WKWebView, etc.)
///        - Elsewhere: reconciles with SDL_GetKeyboardState
///
/// A key can never be stuck for more than one frame regardless of what caused the desync.
///
/// Contract:
///   init()          — once at startup; wires Engine::keys to the internal buffer
///   handle_event()  — every SDL event inside PollEvent; records KEY_DOWN/KEY_UP,
///                     also learns the native keycode mapping for macOS hardware queries
///   sync()          — once per frame AFTER PollEvent; corrects any stuck keys
///   clear()         — explicit full reset (focus loss, overlay transitions, etc.)
void eng_game_keyboard_init(Engine* eng);
void eng_game_keyboard_handle_event(const SDL_Event& ev);
void eng_game_keyboard_sync();
void eng_game_keyboard_clear();
