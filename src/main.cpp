#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <lua.h>
#include <lualib.h>
#include <luacode.h>
#include <sys/stat.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <climits>
#endif

#include "editor_bridge.hpp"
#include "entity_registry.hpp"
#include "webview_host.hpp"

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr int   SCREEN_W  = 900;
static constexpr int   SCREEN_H  = 700;
/// Luau / draw.clear extent (matches #game-surface when inset; else full window logical).
static int             s_game_lu_w = SCREEN_W;
static int             s_game_lu_h = SCREEN_H;
static constexpr float PI        = 3.14159265f;
static constexpr float DEG2RAD   = PI / 180.f;

// ─── Math / draw helpers ──────────────────────────────────────────────────────

struct Vec2 { float x = 0, y = 0; };

inline float randf(float lo, float hi) {
    return lo + (hi - lo) * (rand() / (float)RAND_MAX);
}

static void drawPoly(SDL_Renderer* r,
                     const std::vector<Vec2>& pts,
                     Vec2 pos, float angleDeg,
                     SDL_Color col)
{
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    float a = angleDeg * DEG2RAD;
    float ca = cosf(a), sa = sinf(a);
    size_t n = pts.size();
    for (size_t i = 0; i < n; ++i) {
        Vec2 p0 = pts[i], p1 = pts[(i+1) % n];
        float rx0 = p0.x*ca - p0.y*sa, ry0 = p0.x*sa + p0.y*ca;
        float rx1 = p1.x*ca - p1.y*sa, ry1 = p1.x*sa + p1.y*ca;
        SDL_RenderLine(r, pos.x+rx0, pos.y+ry0, pos.x+rx1, pos.y+ry1);
    }
}

static void drawCircle(SDL_Renderer* r, Vec2 c, float radius, SDL_Color col) {
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    int segs = 20;
    for (int i = 0; i < segs; ++i) {
        float a0 = (i / (float)segs) * 2*PI;
        float a1 = ((i+1) / (float)segs) * 2*PI;
        SDL_RenderLine(r,
            c.x + cosf(a0)*radius, c.y + sinf(a0)*radius,
            c.x + cosf(a1)*radius, c.y + sinf(a1)*radius);
    }
}

static void renderChar(SDL_Renderer* r, char ch, float x, float y, float scale, SDL_Color col) {
    // bits: 0=top 1=top-left 2=top-right 3=mid 4=bot-left 5=bot-right 6=bot
    static const uint8_t seg7[] = {
        0b1110111, 0b0100100, 0b1011101, 0b1101101, 0b0101110,
        0b1101011, 0b1111011, 0b0100101, 0b1111111, 0b1101111,
    };
    if (ch < '0' || ch > '9') return;
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    uint8_t segs = seg7[ch - '0'];
    float w = 6*scale, h = 10*scale, h2 = 5*scale;
    if (segs & (1<<0)) SDL_RenderLine(r, x,   y,    x+w, y);
    if (segs & (1<<1)) SDL_RenderLine(r, x,   y,    x,   y+h2);
    if (segs & (1<<2)) SDL_RenderLine(r, x+w, y,    x+w, y+h2);
    if (segs & (1<<3)) SDL_RenderLine(r, x,   y+h2, x+w, y+h2);
    if (segs & (1<<4)) SDL_RenderLine(r, x,   y+h2, x,   y+h);
    if (segs & (1<<5)) SDL_RenderLine(r, x+w, y+h2, x+w, y+h);
    if (segs & (1<<6)) SDL_RenderLine(r, x,   y+h,  x+w, y+h);
}

static void renderNumber(SDL_Renderer* r, int n, float x, float y, float scale, SDL_Color col) {
    std::string s = std::to_string(n);
    for (size_t i = 0; i < s.size(); ++i)
        renderChar(r, s[i], x + i*8*scale, y, scale, col);
}

// ─── Audio ────────────────────────────────────────────────────────────────────

static constexpr int AUDIO_SR    = 44100;
static constexpr int VOICE_COUNT = 8;

static std::vector<float> genShootSound() {
    int n = (int)(AUDIO_SR * 0.13f);
    std::vector<float> buf(n);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
        float t = i / (float)AUDIO_SR;
        float ft  = 750.f * expf(-t * 14.f) + 180.f;
        phase += ft / AUDIO_SR;
        buf[i] = sinf(2*PI*phase) * expf(-t * 22.f) * 0.5f;
    }
    return buf;
}

static std::vector<float> genNoiseBurst(float dur, float lpAlpha, float decayK, float vol) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float s = 0;
    for (int i = 0; i < n; ++i) {
        float t = i / (float)AUDIO_SR;
        s = s * (1.f - lpAlpha) + randf(-1.f, 1.f) * lpAlpha;
        buf[i] = s * expf(-t * decayK) * vol;
    }
    return buf;
}

static std::vector<float> genDeathSound() {
    int n = (int)(AUDIO_SR * 0.9f);
    std::vector<float> buf(n);
    float phase = 0, ns = 0;
    for (int i = 0; i < n; ++i) {
        float t    = i / (float)AUDIO_SR;
        float env  = expf(-t * 2.8f);
        float freq = 380.f * expf(-t * 3.5f) + 55.f;
        phase += freq / AUDIO_SR;
        ns = ns * 0.86f + randf(-1.f, 1.f) * 0.14f;
        buf[i] = (sinf(2*PI*phase) * 0.45f + ns * 0.55f) * env * 0.75f;
    }
    return buf;
}

static std::vector<float> genBeat(float freq, float dur) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
        float t = i / (float)AUDIO_SR;
        phase  += freq / AUDIO_SR;
        buf[i]  = sinf(2*PI*phase) * expf(-t * 20.f) * 0.45f;
    }
    return buf;
}

static std::vector<float> genThrustChunk(float dur) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float s = 0;
    for (int i = 0; i < n; ++i) {
        float t  = i / (float)AUDIO_SR;
        s = s * 0.87f + randf(-1.f, 1.f) * 0.13f;
        float fade = std::min(t / 0.008f, std::min((dur - t) / 0.008f, 1.f));
        buf[i] = s * fade * 0.28f;
    }
    return buf;
}

struct AudioSystem {
    SDL_AudioDeviceID dev    = 0;
    SDL_AudioSpec     spec   = {};
    SDL_AudioStream*  voices[VOICE_COUNT] = {};
    SDL_AudioStream*  thrustVoice = nullptr;

    std::vector<float> sndShoot, sndExpLarge, sndExpMedium, sndExpSmall, sndDeath;
    std::vector<float> sndBeat[2];

    bool init() {
        spec = { SDL_AUDIO_F32, 1, AUDIO_SR };
        dev  = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (!dev) { SDL_Log("Audio open failed: %s", SDL_GetError()); return false; }
        for (int i = 0; i < VOICE_COUNT; ++i) {
            voices[i] = SDL_CreateAudioStream(&spec, &spec);
            SDL_BindAudioStream(dev, voices[i]);
        }
        thrustVoice = SDL_CreateAudioStream(&spec, &spec);
        SDL_BindAudioStream(dev, thrustVoice);
        sndShoot     = genShootSound();
        sndExpLarge  = genNoiseBurst(0.55f, 0.04f,  5.f, 0.85f);
        sndExpMedium = genNoiseBurst(0.35f, 0.12f,  8.f, 0.75f);
        sndExpSmall  = genNoiseBurst(0.20f, 0.35f, 14.f, 0.65f);
        sndDeath     = genDeathSound();
        sndBeat[0]   = genBeat(120.f, 0.07f);
        sndBeat[1]   = genBeat(90.f,  0.07f);
        return true;
    }

    void shutdown() {
        for (int i = 0; i < VOICE_COUNT; ++i)
            if (voices[i]) SDL_DestroyAudioStream(voices[i]);
        if (thrustVoice) SDL_DestroyAudioStream(thrustVoice);
        if (dev) SDL_CloseAudioDevice(dev);
    }

    void play(const std::vector<float>& samples) {
        if (!dev || samples.empty()) return;
        int bytes = (int)(samples.size() * sizeof(float));
        for (int i = 0; i < VOICE_COUNT; ++i) {
            if (SDL_GetAudioStreamQueued(voices[i]) < 512) {
                SDL_PutAudioStreamData(voices[i], samples.data(), bytes);
                return;
            }
        }
        SDL_ClearAudioStream(voices[0]);
        SDL_PutAudioStreamData(voices[0], samples.data(), bytes);
    }

    void updateThrust(bool on) {
        if (!dev || !on) return;
        int threshold = (int)(AUDIO_SR * 0.08f * sizeof(float));
        if (SDL_GetAudioStreamQueued(thrustVoice) < threshold) {
            auto chunk = genThrustChunk(0.12f);
            SDL_PutAudioStreamData(thrustVoice, chunk.data(),
                (int)(chunk.size() * sizeof(float)));
        }
    }
};

// ─── Engine globals (accessible from Lua callbacks) ───────────────────────────

struct Engine {
    SDL_Window*   window   = nullptr;
    SDL_Renderer* renderer = nullptr;
    AudioSystem   audio;
    const bool*   keys     = nullptr;
    bool          quit     = false;
};

static Engine         g_eng;
static EntityRegistry g_entities;

static std::atomic<bool> s_script_reload_requested{false};
static std::atomic<bool> s_script_paused{false};
static std::atomic<bool> s_start_sim_requested{false};

static void on_script_reload_request() {
    s_script_reload_requested.store(true, std::memory_order_relaxed);
}

static void on_script_set_paused(bool paused) {
    s_script_paused.store(paused, std::memory_order_relaxed);
}

static void on_script_start_sim_request() {
    s_start_sim_requested.store(true, std::memory_order_relaxed);
}

// Game region from web UI in CSS/layout coords + its reference UI space.
static int s_ui_game_x = 0, s_ui_game_y = 0, s_ui_game_w = 0, s_ui_game_h = 0;
static int s_ui_space_w = 0, s_ui_space_h = 0;

static void on_web_game_rect(int x, int y, int w, int h, int ui_space_w, int ui_space_h, void* /*user*/) {
    s_ui_game_x = x;
    s_ui_game_y = y;
    s_ui_game_w = w;
    s_ui_game_h = h;
    s_ui_space_w = ui_space_w;
    s_ui_space_h = ui_space_h;
}

static void sync_lua_screen_size(lua_State* L, int w, int h) {
    if (w < 1)
        w = SCREEN_W;
    if (h < 1)
        h = SCREEN_H;
    s_game_lu_w = w;
    s_game_lu_h = h;
    lua_getglobal(L, "screen");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }
    lua_pushinteger(L, (lua_Integer)w);
    lua_setfield(L, -2, "w");
    lua_pushinteger(L, (lua_Integer)h);
    lua_setfield(L, -2, "h");
    lua_pop(L, 1);
}

static std::string web_ui_root_path() {
    // Vite production bundle (npm run build in /web). For HMR dev, set WEBVIEW_DEV_URL and run
    // `npm run dev` in /web (e.g. WEBVIEW_DEV_URL=http://127.0.0.1:5173/).
    if (const char* dev = getenv("WEBVIEW_DEV_URL"))
        if (dev[0] != '\0')
            return std::string(dev);
    std::string p = std::string(SDL_GetBasePath()) + "../web/dist";
#if defined(__APPLE__)
    char resolved[PATH_MAX];
    if (realpath(p.c_str(), resolved))
        p = resolved;
#endif
    return p;
}

// ─── Lua helpers ──────────────────────────────────────────────────────────────

static SDL_Color luaColor(lua_State* L, int r, int g, int b, int a) {
    return {
        (Uint8)luaL_optinteger(L, r, 255),
        (Uint8)luaL_optinteger(L, g, 255),
        (Uint8)luaL_optinteger(L, b, 255),
        (Uint8)luaL_optinteger(L, a, 255),
    };
}

// ─── Lua bindings: draw ───────────────────────────────────────────────────────

// draw.clear(r, g, b) — fill logical screen rect only (SDL_RenderClear ignores viewport in SDL3).
static int l_draw_clear(lua_State* L) {
    Uint8 r = (Uint8)luaL_optinteger(L, 1, 0);
    Uint8 g = (Uint8)luaL_optinteger(L, 2, 0);
    Uint8 b = (Uint8)luaL_optinteger(L, 3, 0);
    SDL_SetRenderDrawColor(g_eng.renderer, r, g, b, 255);
    SDL_FRect bg = { 0.f, 0.f, (float)s_game_lu_w, (float)s_game_lu_h };
    SDL_RenderFillRect(g_eng.renderer, &bg);
    return 0;
}

// draw.line(x1, y1, x2, y2, r, g, b, a)
static int l_draw_line(lua_State* L) {
    float x1 = (float)luaL_checknumber(L, 1);
    float y1 = (float)luaL_checknumber(L, 2);
    float x2 = (float)luaL_checknumber(L, 3);
    float y2 = (float)luaL_checknumber(L, 4);
    SDL_SetRenderDrawColor(g_eng.renderer,
        (Uint8)luaL_optinteger(L, 5, 255),
        (Uint8)luaL_optinteger(L, 6, 255),
        (Uint8)luaL_optinteger(L, 7, 255),
        (Uint8)luaL_optinteger(L, 8, 255));
    SDL_RenderLine(g_eng.renderer, x1, y1, x2, y2);
    return 0;
}

// draw.circle(x, y, radius, r, g, b, a)
static int l_draw_circle(lua_State* L) {
    float cx     = (float)luaL_checknumber(L, 1);
    float cy     = (float)luaL_checknumber(L, 2);
    float radius = (float)luaL_checknumber(L, 3);
    SDL_Color col = luaColor(L, 4, 5, 6, 7);
    drawCircle(g_eng.renderer, {cx, cy}, radius, col);
    return 0;
}

// draw.poly(verts, x, y, angle, r, g, b, a)
// verts = {{x,y}, {x,y}, ...}
static int l_draw_poly(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    std::vector<Vec2> pts;
    lua_pushnil(L);
    while (lua_next(L, 1)) {
        lua_rawgeti(L, -1, 1); float x = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        lua_rawgeti(L, -1, 2); float y = (float)lua_tonumber(L, -1); lua_pop(L, 1);
        pts.push_back({x, y});
        lua_pop(L, 1);
    }
    float px    = (float)luaL_checknumber(L, 2);
    float py    = (float)luaL_checknumber(L, 3);
    float angle = (float)luaL_checknumber(L, 4);
    SDL_Color col = luaColor(L, 5, 6, 7, 8);
    drawPoly(g_eng.renderer, pts, {px, py}, angle, col);
    return 0;
}

// draw.char(c, x, y, scale, r, g, b, a)
static int l_draw_char(lua_State* L) {
    const char* s  = luaL_checkstring(L, 1);
    float x        = (float)luaL_checknumber(L, 2);
    float y        = (float)luaL_checknumber(L, 3);
    float scale    = (float)luaL_optnumber(L, 4, 1.0);
    SDL_Color col  = luaColor(L, 5, 6, 7, 8);
    renderChar(g_eng.renderer, s[0], x, y, scale, col);
    return 0;
}

// draw.number(n, x, y, scale, r, g, b, a)
static int l_draw_number(lua_State* L) {
    int   n     = (int)luaL_checkinteger(L, 1);
    float x     = (float)luaL_checknumber(L, 2);
    float y     = (float)luaL_checknumber(L, 3);
    float scale = (float)luaL_optnumber(L, 4, 1.0);
    SDL_Color col = luaColor(L, 5, 6, 7, 8);
    renderNumber(g_eng.renderer, n, x, y, scale, col);
    return 0;
}

// draw.present()  — flip the framebuffer
static int l_draw_present(lua_State* /*L*/) {
    SDL_RenderPresent(g_eng.renderer);
    return 0;
}

// ─── Lua bindings: input ──────────────────────────────────────────────────────

// input.down("left"|"right"|"up"|"down"|"space"|"a"|"d"|"w"|"r"|"escape")
static int l_input_down(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    SDL_Scancode sc  = SDL_SCANCODE_UNKNOWN;
    if      (!strcmp(name, "left"))   sc = SDL_SCANCODE_LEFT;
    else if (!strcmp(name, "right"))  sc = SDL_SCANCODE_RIGHT;
    else if (!strcmp(name, "up"))     sc = SDL_SCANCODE_UP;
    else if (!strcmp(name, "down"))   sc = SDL_SCANCODE_DOWN;
    else if (!strcmp(name, "space"))  sc = SDL_SCANCODE_SPACE;
    else if (!strcmp(name, "a"))      sc = SDL_SCANCODE_A;
    else if (!strcmp(name, "d"))      sc = SDL_SCANCODE_D;
    else if (!strcmp(name, "w"))      sc = SDL_SCANCODE_W;
    else if (!strcmp(name, "r"))      sc = SDL_SCANCODE_R;
    else if (!strcmp(name, "escape")) sc = SDL_SCANCODE_ESCAPE;
    lua_pushboolean(L, sc != SDL_SCANCODE_UNKNOWN && g_eng.keys && g_eng.keys[sc]);
    return 1;
}

// ─── Lua bindings: audio ──────────────────────────────────────────────────────

// audio.play("shoot"|"exp_large"|"exp_medium"|"exp_small"|"death"|"beat0"|"beat1")
static int l_audio_play(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    AudioSystem& a = g_eng.audio;
    if      (!strcmp(name, "shoot"))      a.play(a.sndShoot);
    else if (!strcmp(name, "exp_large"))  a.play(a.sndExpLarge);
    else if (!strcmp(name, "exp_medium")) a.play(a.sndExpMedium);
    else if (!strcmp(name, "exp_small"))  a.play(a.sndExpSmall);
    else if (!strcmp(name, "death"))      a.play(a.sndDeath);
    else if (!strcmp(name, "beat0"))      a.play(a.sndBeat[0]);
    else if (!strcmp(name, "beat1"))      a.play(a.sndBeat[1]);
    return 0;
}

// audio.thrust(on)
static int l_audio_thrust(lua_State* L) {
    bool on = lua_toboolean(L, 1);
    g_eng.audio.updateThrust(on);
    return 0;
}

// ─── Lua bindings: app ────────────────────────────────────────────────────────

static int l_app_quit(lua_State* /*L*/) {
    g_eng.quit = true;
    return 0;
}

// engine.entities.add(id, name, x, y, angle, vx, vy)
static int l_engine_entities_add(lua_State* L) {
    const char* id   = luaL_checkstring(L, 1);
    const char* name = luaL_checkstring(L, 2);
    float       x    = (float)luaL_checknumber(L, 3);
    float       y    = (float)luaL_checknumber(L, 4);
    float       ang  = (float)luaL_checknumber(L, 5);
    float       vx   = (float)luaL_checknumber(L, 6);
    float       vy   = (float)luaL_checknumber(L, 7);
    g_entities.add(id, name, x, y, ang, vx, vy);
    return 0;
}

// ─── Lua: require() ──────────────────────────────────────────────────────────

static int l_require(lua_State* L) {
    const char* modname = luaL_checkstring(L, 1);

    // Return cached module if already loaded
    lua_getglobal(L, "package");
    lua_getfield(L, -1, "loaded");
    lua_getfield(L, -1, modname);
    if (!lua_isnil(L, -1))
        return 1;  // cached value on top; Lua ignores the rest
    lua_pop(L, 1);  // pop nil

    // Resolve path: basepath .. modname .. ".lua"
    lua_getfield(L, -2, "basepath");
    const char* base = lua_tostring(L, -1);
    if (!base) luaL_error(L, "package.basepath not set");
    std::string path = std::string(base) + modname + ".lua";
    lua_pop(L, 1);

    // Read source file
    std::ifstream file(path);
    if (!file.is_open())
        luaL_error(L, "module '%s' not found (tried '%s')", modname, path.c_str());
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Compile to Luau bytecode
    size_t bytecodeSize = 0;
    char*  bytecode     = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);
    if (!bytecode) luaL_error(L, "require: out of memory compiling '%s'", modname);

    // Load bytecode (compile errors surface here)
    std::string chunkName = std::string("=") + modname;
    int loadResult = luau_load(L, chunkName.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);
    if (loadResult != LUA_OK)
        luaL_error(L, "require: load error in '%s': %s", modname, lua_tostring(L, -1));

    // Execute module chunk; expect 1 return value
    if (lua_pcall(L, 0, 1, 0) != LUA_OK)
        luaL_error(L, "require: error in '%s': %s", modname, lua_tostring(L, -1));

    // Modules that return nil get stored as true so we know they ran
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_pushboolean(L, 1);
    }

    // Cache: package.loaded[modname] = result
    // Stack: ... package, loaded, result
    lua_pushvalue(L, -1);         // dup result
    lua_setfield(L, -3, modname); // loaded[modname] = result; pops dup
    lua_remove(L, -2);            // remove loaded
    lua_remove(L, -2);            // remove package
    return 1;
}

// ─── Register all APIs ────────────────────────────────────────────────────────

static void registerAPI(lua_State* L) {
    // draw.*
    lua_newtable(L);
    lua_pushcfunction(L, l_draw_clear,   "draw.clear");   lua_setfield(L, -2, "clear");
    lua_pushcfunction(L, l_draw_line,    "draw.line");    lua_setfield(L, -2, "line");
    lua_pushcfunction(L, l_draw_circle,  "draw.circle");  lua_setfield(L, -2, "circle");
    lua_pushcfunction(L, l_draw_poly,    "draw.poly");    lua_setfield(L, -2, "poly");
    lua_pushcfunction(L, l_draw_char,    "draw.char");    lua_setfield(L, -2, "char");
    lua_pushcfunction(L, l_draw_number,  "draw.number");  lua_setfield(L, -2, "number");
    lua_pushcfunction(L, l_draw_present, "draw.present"); lua_setfield(L, -2, "present");
    lua_setglobal(L, "draw");

    // input.*
    lua_newtable(L);
    lua_pushcfunction(L, l_input_down, "input.down"); lua_setfield(L, -2, "down");
    lua_setglobal(L, "input");

    // audio.*
    lua_newtable(L);
    lua_pushcfunction(L, l_audio_play,   "audio.play");   lua_setfield(L, -2, "play");
    lua_pushcfunction(L, l_audio_thrust, "audio.thrust"); lua_setfield(L, -2, "thrust");
    lua_setglobal(L, "audio");

    // screen
    lua_newtable(L);
    lua_pushinteger(L, SCREEN_W); lua_setfield(L, -2, "w");
    lua_pushinteger(L, SCREEN_H); lua_setfield(L, -2, "h");
    lua_setglobal(L, "screen");

    // app.*
    lua_newtable(L);
    lua_pushcfunction(L, l_app_quit, "app.quit"); lua_setfield(L, -2, "quit");
    lua_setglobal(L, "app");

    // engine.* (native services; scripts sync entity registry after world.rebuild)
    lua_newtable(L);
    lua_newtable(L);
    lua_pushcfunction(L, l_engine_entities_add, "engine.entities.add");
    lua_setfield(L, -2, "add");
    lua_setfield(L, -2, "entities");
    lua_setglobal(L, "engine");

    // require
    lua_pushcfunction(L, l_require, "require");
    lua_setglobal(L, "require");
}

// ─── Lua helpers ──────────────────────────────────────────────────────────────

static time_t fileMtime(const std::string& path) {
    struct stat st;
    return (stat(path.c_str(), &st) == 0) ? st.st_mtime : 0;
}

static lua_State* createLuaState(const std::string& scriptPath) {
    // Read source from disk
    std::ifstream file(scriptPath);
    if (!file.is_open()) {
        SDL_Log("Cannot open script: %s", scriptPath.c_str());
        return nullptr;
    }
    std::string source((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Derive script directory so require() can find sibling modules
    std::string baseDir = scriptPath;
    size_t slash = baseDir.find_last_of("/\\");
    baseDir = (slash != std::string::npos) ? baseDir.substr(0, slash + 1) : "./";

    // Compile source to Luau bytecode
    size_t bytecodeSize = 0;
    char*  bytecode     = luau_compile(source.c_str(), source.size(), nullptr, &bytecodeSize);
    if (!bytecode) {
        SDL_Log("Luau compile: out of memory");
        return nullptr;
    }

    // Set up state and load bytecode
    lua_State* L = luaL_newstate();
    luaL_openlibs(L);
    registerAPI(L);

    // package table used by require()
    lua_newtable(L);
    lua_newtable(L);                            lua_setfield(L, -2, "loaded");
    lua_pushstring(L, baseDir.c_str());         lua_setfield(L, -2, "basepath");
    lua_setglobal(L, "package");

    int loadResult = luau_load(L, scriptPath.c_str(), bytecode, bytecodeSize, 0);
    free(bytecode);

    if (loadResult != 0) {
        SDL_Log("Luau load error: %s", lua_tostring(L, -1));
        lua_close(L);
        return nullptr;
    }

    // Execute top-level chunk (defines all functions / runs init code)
    if (lua_pcall(L, 0, 0, 0) != 0) {
        SDL_Log("Luau exec error: %s", lua_tostring(L, -1));
        lua_close(L);
        return nullptr;
    }

    return L;
}

// Optional: `--capture-window-png path.png` + `--capture-after-frames N` (composited window on macOS).
static int         s_capture_after_frames = -1;
static const char* s_capture_path         = nullptr;
// Debug: compute game rect from WK layout (same % as web/index.html #game-surface) instead of JS.
static bool        s_native_game_rect_pct = false;

static void parse_cli(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--capture-after-frames") == 0 && i + 1 < argc) {
            s_capture_after_frames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--capture-window-png") == 0 && i + 1 < argc) {
            s_capture_path = argv[++i];
        } else if (std::strcmp(argv[i], "--native-game-rect") == 0) {
            s_native_game_rect_pct = true;
        }
    }
    if (s_capture_path && s_capture_after_frames < 0)
        s_capture_after_frames = 180;
}

static bool luaCall(lua_State* L, const char* fn, int nargs, int nret) {
    lua_getglobal(L, fn);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1 + nargs);
        return false;
    }
    // move function before args already on stack
    if (nargs > 0) lua_insert(L, -(nargs + 1));
    if (lua_pcall(L, nargs, nret, 0) != LUA_OK) {
        SDL_Log("Lua error in %s: %s", fn, lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }
    return true;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    parse_cli(argc, argv);
    srand(static_cast<unsigned>(time(nullptr)));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    g_eng.window   = SDL_CreateWindow("Asteroids", SCREEN_W, SCREEN_H, SDL_WINDOW_RESIZABLE);
    g_eng.renderer = SDL_CreateRenderer(g_eng.window, nullptr);
    if (!g_eng.window || !g_eng.renderer) {
        SDL_Log("Window/Renderer failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    SDL_SetRenderVSync(g_eng.renderer, 0);
    g_eng.audio.init();

    webview_host_set_game_rect_callback(on_web_game_rect, nullptr);
    {
        std::string luaDir = std::string(SDL_GetBasePath()) + "../lua";
        webview_host_set_lua_workspace(luaDir.c_str());
    }
    webview_host_set_script_controls(
        on_script_reload_request, on_script_set_paused, on_script_start_sim_request);
    {
        std::string webRoot = web_ui_root_path();
        if (!webview_host_init(g_eng.window, webRoot.c_str()))
            SDL_Log("Web overlay init failed (tried %s)", webRoot.c_str());
    }

    // ── Lua setup ─────────────────────────────────────────────────────────
    std::string scriptPath = SDL_GetBasePath();
    scriptPath += "../lua/game.lua";

    lua_State* L = createLuaState(scriptPath);
    if (!L) {
        SDL_Quit();
        return 1;
    }

    luaCall(L, "_init", 0, 0);

    // ── Game loop ─────────────────────────────────────────────────────────
    float  totalTime   = 0;
    time_t scriptMtime = fileMtime(scriptPath);
    float  reloadCheck = 0;
    Uint64 prev        = SDL_GetTicks();
    int    frame_idx   = 0;

    while (!g_eng.quit) {
        frame_idx++;
        Uint64 now = SDL_GetTicks();
        float  dt  = (now - prev) / 1000.f;
        prev       = now;
        if (dt > 0.05f) dt = 0.05f;
        totalTime += dt;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) {
                g_eng.quit = true;
            } else if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
                int pw = 0, ph = 0;
                SDL_GetWindowSizeInPixels(g_eng.window, &pw, &ph);
                webview_host_on_window_resized(pw, ph);
            } else if (ev.type == SDL_EVENT_KEY_DOWN) {
                const char* keyName = nullptr;
                switch (ev.key.key) {
                    case SDLK_LEFT:   keyName = "left";   break;
                    case SDLK_RIGHT:  keyName = "right";  break;
                    case SDLK_UP:     keyName = "up";     break;
                    case SDLK_DOWN:   keyName = "down";   break;
                    case SDLK_SPACE:  keyName = "space";  break;
                    case SDLK_A:      keyName = "a";      break;
                    case SDLK_D:      keyName = "d";      break;
                    case SDLK_W:      keyName = "w";      break;
                    case SDLK_R:      keyName = "r";      break;
                    case SDLK_ESCAPE: keyName = "escape"; break;
                    default: break;
                }
                if (keyName) {
                    lua_pushstring(L, keyName);
                    luaCall(L, "_on_keydown", 1, 0);
                }
            }
        }

        g_eng.keys = SDL_GetKeyboardState(nullptr);

        // ── Hot-reload ────────────────────────────────────────────────────
        reloadCheck -= dt;
        if (reloadCheck <= 0) {
            reloadCheck = 0.25f;
            time_t mtime = fileMtime(scriptPath);
            if (mtime != scriptMtime) {
                scriptMtime = mtime;
                lua_State* newL = createLuaState(scriptPath);
                if (newL) {
                    lua_close(L);
                    L = newL;
                    luaCall(L, "_init", 0, 0);
                    s_script_paused.store(false, std::memory_order_relaxed);
                    SDL_Log("game.lua reloaded");
                }
                // on failure: keep running with the old state
            }
        }

        if (s_script_reload_requested.exchange(false, std::memory_order_acq_rel)) {
            lua_State* newL = createLuaState(scriptPath);
            if (newL) {
                lua_close(L);
                L                = newL;
                scriptMtime      = fileMtime(scriptPath);
                luaCall(L, "_init", 0, 0);
                s_script_paused.store(false, std::memory_order_relaxed);
                SDL_Log("game.lua restarted (editor)");
            }
        }

        if (s_start_sim_requested.exchange(false, std::memory_order_acq_rel)) {
            s_script_paused.store(false, std::memory_order_relaxed);
            luaCall(L, "_on_hud_play", 0, 0);
        }

        webview_host_poll_dom_layout();

        int lu_w = SCREEN_W, lu_h = SCREEN_H;
        webview_apply_game_viewport(g_eng.renderer, g_eng.window,
            SCREEN_W, SCREEN_H,
            s_ui_game_x, s_ui_game_y, s_ui_game_w, s_ui_game_h,
            s_ui_space_w, s_ui_space_h, s_native_game_rect_pct,
            &lu_w, &lu_h);
        sync_lua_screen_size(L, lu_w, lu_h);

        g_entities.clear();
        if (!s_script_paused.load(std::memory_order_relaxed)) {
            lua_pushnumber(L, dt);
            lua_pushnumber(L, totalTime);
            luaCall(L, "_update", 2, 0);
        }
        editor_bridge_publish_entity_snapshot(g_entities);

        lua_pushnumber(L, totalTime);
        luaCall(L, "_render", 1, 0);

#if defined(__APPLE__)
        if (s_capture_path && s_capture_after_frames > 0 && frame_idx == s_capture_after_frames) {
            // Let WKWebView paint chrome + layout before capture.
            SDL_Delay(800);
            if (!webview_host_capture_composite_png(g_eng.window, s_capture_path))
                SDL_Log("capture: failed (path=%s)", s_capture_path);
            else
                SDL_Log("capture: screencapture wrote %s", s_capture_path);
            g_eng.quit = true;
        }
#endif
    }

    lua_close(L);
    g_eng.audio.shutdown();
    webview_host_shutdown();
    SDL_DestroyRenderer(g_eng.renderer);
    SDL_DestroyWindow(g_eng.window);
    SDL_Quit();
    return 0;
}
