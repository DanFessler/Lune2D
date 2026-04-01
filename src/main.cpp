#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

// ─── Constants ────────────────────────────────────────────────────────────────

static constexpr int   SCREEN_W      = 900;
static constexpr int   SCREEN_H      = 700;
static constexpr float PI            = 3.14159265f;
static constexpr float DEG2RAD       = PI / 180.f;

// Ship
static constexpr float SHIP_ROT_SPD  = 200.f;
static constexpr float SHIP_THRUST   = 250.f;
static constexpr float SHIP_MAX_SPD  = 400.f;
static constexpr float SHIP_RADIUS   = 14.f;

// Bullets
static constexpr float BULLET_SPD    = 520.f;
static constexpr float BULLET_LIFE   = 1.1f;
static constexpr float BULLET_RADIUS = 3.f;
static constexpr float SHOOT_COOLDOWN= 0.18f;

// Asteroids
static constexpr float AST_LARGE_R   = 42.f;
static constexpr float AST_MEDIUM_R  = 24.f;
static constexpr float AST_SMALL_R   = 13.f;
static constexpr float AST_MIN_SPD   = 40.f;
static constexpr float AST_MAX_SPD   = 110.f;

// Scoring
static constexpr int   SCORE_LARGE   = 20;
static constexpr int   SCORE_MEDIUM  = 50;
static constexpr int   SCORE_SMALL   = 100;

// ─── Math helpers ─────────────────────────────────────────────────────────────

struct Vec2 { float x = 0, y = 0; };

inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x+b.x, a.y+b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x-b.x, a.y-b.y}; }
inline Vec2 operator*(Vec2 a, float s){ return {a.x*s, a.y*s}; }
inline Vec2& operator+=(Vec2& a, Vec2 b){ a.x+=b.x; a.y+=b.y; return a; }
inline Vec2& operator*=(Vec2& a, float s){ a.x*=s; a.y*=s; return a; }

inline float dist2(Vec2 a, Vec2 b) {
    float dx = a.x-b.x, dy = a.y-b.y;
    return dx*dx + dy*dy;
}

inline Vec2 wrap(Vec2 p) {
    if (p.x < 0) p.x += SCREEN_W;
    if (p.x > SCREEN_W) p.x -= SCREEN_W;
    if (p.y < 0) p.y += SCREEN_H;
    if (p.y > SCREEN_H) p.y -= SCREEN_H;
    return p;
}

inline float randf(float lo, float hi) {
    return lo + (hi - lo) * (rand() / (float)RAND_MAX);
}

// ─── Audio ────────────────────────────────────────────────────────────────────

static constexpr int AUDIO_SR    = 44100;
static constexpr int VOICE_COUNT = 8;

// Synthesised sounds — all generated at startup, no files needed

static std::vector<float> genShootSound() {
    // Short descending swept tone
    int n = (int)(AUDIO_SR * 0.13f);
    std::vector<float> buf(n);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
        float t = i / (float)AUDIO_SR;
        float env = expf(-t * 22.f);
        float ft  = 750.f * expf(-t * 14.f) + 180.f;
        phase += ft / AUDIO_SR;
        buf[i] = sinf(2*PI*phase) * env * 0.5f;
    }
    return buf;
}

static std::vector<float> genNoiseBurst(float dur, float lpAlpha, float decayK, float vol) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float s = 0;
    for (int i = 0; i < n; ++i) {
        float t   = i / (float)AUDIO_SR;
        float env = expf(-t * decayK);
        float raw = randf(-1.f, 1.f);
        s = s * (1.f - lpAlpha) + raw * lpAlpha;
        buf[i] = s * env * vol;
    }
    return buf;
}

static std::vector<float> genDeathSound() {
    // Descending tone + noise
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

    std::vector<float> sndShoot;
    std::vector<float> sndExpLarge;
    std::vector<float> sndExpMedium;
    std::vector<float> sndExpSmall;
    std::vector<float> sndDeath;
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
        // All busy — steal voice 0
        SDL_ClearAudioStream(voices[0]);
        SDL_PutAudioStreamData(voices[0], samples.data(), bytes);
    }

    void updateThrust(bool on) {
        if (!dev) return;
        if (!on) return;  // let the current chunk drain naturally
        // Keep ~80ms queued to avoid gaps without over-buffering
        int threshold = (int)(AUDIO_SR * 0.08f * sizeof(float));
        if (SDL_GetAudioStreamQueued(thrustVoice) < threshold) {
            auto chunk = genThrustChunk(0.12f);
            SDL_PutAudioStreamData(thrustVoice, chunk.data(),
                (int)(chunk.size() * sizeof(float)));
        }
    }

    void playShoot()          { play(sndShoot); }
    void playBeat(int idx)    { play(sndBeat[idx & 1]); }
    void playDeath()          { play(sndDeath); }
    void playExplosion(int sz) {
        if      (sz == 0) play(sndExpLarge);
        else if (sz == 1) play(sndExpMedium);
        else              play(sndExpSmall);
    }
};

// ─── Draw helpers ─────────────────────────────────────────────────────────────

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
    SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);
    // bits: 0=top 1=top-left 2=top-right 3=mid 4=bot-left 5=bot-right 6=bot
    static const uint8_t seg7[] = {
        0b1110111, // 0
        0b0100100, // 1
        0b1011101, // 2
        0b1101101, // 3
        0b0101110, // 4
        0b1101011, // 5
        0b1111011, // 6
        0b0100101, // 7
        0b1111111, // 8
        0b1101111, // 9
    };
    if (ch < '0' || ch > '9') return;
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

// ─── Game entities ────────────────────────────────────────────────────────────

struct Bullet {
    Vec2  pos;
    Vec2  vel;
    float life = BULLET_LIFE;
    bool  alive = true;
};

enum class AsteroidSize { Large, Medium, Small };

static float sizeToRadius(AsteroidSize s) {
    switch(s) {
        case AsteroidSize::Large:  return AST_LARGE_R;
        case AsteroidSize::Medium: return AST_MEDIUM_R;
        default:                   return AST_SMALL_R;
    }
}

static int sizeToScore(AsteroidSize s) {
    switch(s) {
        case AsteroidSize::Large:  return SCORE_LARGE;
        case AsteroidSize::Medium: return SCORE_MEDIUM;
        default:                   return SCORE_SMALL;
    }
}

struct Asteroid {
    Vec2         pos;
    Vec2         vel;
    float        angle    = 0;
    float        rotSpeed = 0;
    AsteroidSize size;
    bool         alive    = true;
    std::vector<Vec2> shape;

    static Asteroid make(Vec2 pos, AsteroidSize sz, Vec2 vel = {0,0}) {
        Asteroid a;
        a.pos  = pos;
        a.size = sz;
        float r = sizeToRadius(sz);
        if (vel.x == 0 && vel.y == 0) {
            float spd = randf(AST_MIN_SPD, AST_MAX_SPD);
            float ang = randf(0, 2*PI);
            a.vel = {cosf(ang)*spd, sinf(ang)*spd};
        } else {
            a.vel = vel;
        }
        a.angle    = randf(0, 360);
        a.rotSpeed = randf(20, 90) * (rand()%2 ? 1 : -1);
        int verts  = 10 + rand()%4;
        for (int i = 0; i < verts; ++i) {
            float ang = (i / (float)verts) * 2 * PI;
            float ri  = r * randf(0.6f, 1.0f);
            a.shape.push_back({cosf(ang)*ri, sinf(ang)*ri});
        }
        return a;
    }
};

struct Ship {
    Vec2  pos  = {SCREEN_W/2.f, SCREEN_H/2.f};
    Vec2  vel  = {0, 0};
    float angle = 270.f;
    bool  thrusting = false;
    float shootTimer = 0;
    bool  alive = true;
    float respawnTimer = 0;
    int   lives = 3;

    static std::vector<Vec2> shape() {
        return { {0, -SHIP_RADIUS*1.3f}, {SHIP_RADIUS, SHIP_RADIUS}, {-SHIP_RADIUS, SHIP_RADIUS} };
    }
    static std::vector<Vec2> flameShape() {
        return { {SHIP_RADIUS*0.6f, SHIP_RADIUS}, {0, SHIP_RADIUS*2.1f}, {-SHIP_RADIUS*0.6f, SHIP_RADIUS} };
    }
};

// ─── Spawn helpers ────────────────────────────────────────────────────────────

static Vec2 randomEdgePos() {
    int edge = rand() % 4;
    switch(edge) {
        case 0: return {randf(0, SCREEN_W), -AST_LARGE_R};
        case 1: return {randf(0, SCREEN_W),  SCREEN_H + AST_LARGE_R};
        case 2: return {-AST_LARGE_R,        randf(0, SCREEN_H)};
        default:return { SCREEN_W + AST_LARGE_R, randf(0, SCREEN_H)};
    }
}

static void spawnWave(std::vector<Asteroid>& asts, int count) {
    for (int i = 0; i < count; ++i)
        asts.push_back(Asteroid::make(randomEdgePos(), AsteroidSize::Large));
}

// ─── Game state ───────────────────────────────────────────────────────────────

enum class GameState { Playing, GameOver, NewWave };

struct Game {
    Ship                  ship;
    std::vector<Bullet>   bullets;
    std::vector<Asteroid> asteroids;
    int       score     = 0;
    int       wave      = 0;
    float     waveDelay = 0;
    GameState state     = GameState::NewWave;
    float     beatTimer = 0;
    int       beatIndex = 0;

    void reset() {
        ship = Ship{};
        bullets.clear();
        asteroids.clear();
        score     = 0;
        wave      = 0;
        state     = GameState::NewWave;
        waveDelay = 1.5f;
        beatTimer = 0;
        beatIndex = 0;
    }
};

// ─── Update ───────────────────────────────────────────────────────────────────

static void update(Game& g, float dt,
                   bool left, bool right, bool thrust, bool shoot,
                   AudioSystem& audio)
{
    if (g.state == GameState::GameOver) return;

    if (g.state == GameState::NewWave) {
        g.waveDelay -= dt;
        if (g.waveDelay <= 0) {
            g.wave++;
            spawnWave(g.asteroids, 2 + g.wave);
            g.state = GameState::Playing;
        }
        return;
    }

    // ── Ship ──────────────────────────────────────────────────────────────
    Ship& s = g.ship;
    s.shootTimer -= dt;
    if (s.respawnTimer > 0) s.respawnTimer -= dt;

    if (s.alive) {
        if (left)  s.angle -= SHIP_ROT_SPD * dt;
        if (right) s.angle += SHIP_ROT_SPD * dt;

        s.thrusting = thrust;
        if (thrust) {
            float rad = (s.angle - 90.f) * DEG2RAD;
            s.vel.x += cosf(rad) * SHIP_THRUST * dt;
            s.vel.y += sinf(rad) * SHIP_THRUST * dt;
        }
        float spd2 = s.vel.x*s.vel.x + s.vel.y*s.vel.y;
        if (spd2 > SHIP_MAX_SPD*SHIP_MAX_SPD) {
            float sc = SHIP_MAX_SPD / sqrtf(spd2);
            s.vel *= sc;
        }
        s.pos += s.vel * dt;
        s.pos  = wrap(s.pos);

        if (shoot && s.shootTimer <= 0) {
            s.shootTimer = SHOOT_COOLDOWN;
            float rad = (s.angle - 90.f) * DEG2RAD;
            Vec2 tip = {s.pos.x + cosf(rad)*SHIP_RADIUS*1.3f,
                        s.pos.y + sinf(rad)*SHIP_RADIUS*1.3f};
            Bullet b;
            b.pos = tip;
            b.vel = {s.vel.x + cosf(rad)*BULLET_SPD,
                     s.vel.y + sinf(rad)*BULLET_SPD};
            g.bullets.push_back(b);
            audio.playShoot();
        }
    }

    // ── Bullets ───────────────────────────────────────────────────────────
    for (auto& b : g.bullets) {
        if (!b.alive) continue;
        b.life -= dt;
        if (b.life <= 0) { b.alive = false; continue; }
        b.pos += b.vel * dt;
        b.pos  = wrap(b.pos);
    }

    // ── Asteroids ─────────────────────────────────────────────────────────
    for (auto& a : g.asteroids) {
        if (!a.alive) continue;
        a.pos   += a.vel * dt;
        a.pos    = wrap(a.pos);
        a.angle += a.rotSpeed * dt;
    }

    // ── Bullet × Asteroid collision ───────────────────────────────────────
    std::vector<Asteroid> newAsts;
    for (auto& b : g.bullets) {
        if (!b.alive) continue;
        for (auto& a : g.asteroids) {
            if (!a.alive) continue;
            float r = sizeToRadius(a.size);
            if (dist2(b.pos, a.pos) < (r+BULLET_RADIUS)*(r+BULLET_RADIUS)) {
                b.alive = false;
                a.alive = false;
                g.score += sizeToScore(a.size);
                audio.playExplosion((int)a.size);
                if (a.size == AsteroidSize::Large) {
                    for (int i = 0; i < 2; ++i) {
                        float spd = randf(AST_MIN_SPD*1.2f, AST_MAX_SPD*1.2f);
                        float ang = randf(0, 2*PI);
                        newAsts.push_back(Asteroid::make(a.pos, AsteroidSize::Medium,
                            {cosf(ang)*spd, sinf(ang)*spd}));
                    }
                } else if (a.size == AsteroidSize::Medium) {
                    for (int i = 0; i < 2; ++i) {
                        float spd = randf(AST_MIN_SPD*1.5f, AST_MAX_SPD*1.5f);
                        float ang = randf(0, 2*PI);
                        newAsts.push_back(Asteroid::make(a.pos, AsteroidSize::Small,
                            {cosf(ang)*spd, sinf(ang)*spd}));
                    }
                }
                break;
            }
        }
    }
    for (auto& na : newAsts) g.asteroids.push_back(na);

    // ── Ship × Asteroid collision ─────────────────────────────────────────
    if (s.alive && s.respawnTimer <= 0) {
        for (auto& a : g.asteroids) {
            if (!a.alive) continue;
            float r = sizeToRadius(a.size) + SHIP_RADIUS;
            if (dist2(s.pos, a.pos) < r*r) {
                audio.playDeath();
                s.lives--;
                if (s.lives <= 0) {
                    s.alive = false;
                    g.state = GameState::GameOver;
                } else {
                    s.pos          = {SCREEN_W/2.f, SCREEN_H/2.f};
                    s.vel          = {0, 0};
                    s.angle        = 270.f;
                    s.respawnTimer = 2.5f;
                }
                break;
            }
        }
    }

    // ── Cull dead objects ─────────────────────────────────────────────────
    g.bullets.erase(std::remove_if(g.bullets.begin(), g.bullets.end(),
        [](const Bullet& b){ return !b.alive; }), g.bullets.end());
    g.asteroids.erase(std::remove_if(g.asteroids.begin(), g.asteroids.end(),
        [](const Asteroid& a){ return !a.alive; }), g.asteroids.end());

    // ── Wave clear ────────────────────────────────────────────────────────
    if (g.asteroids.empty() && g.state == GameState::Playing) {
        g.state     = GameState::NewWave;
        g.waveDelay = 2.0f;
    }

    // ── Heartbeat ─────────────────────────────────────────────────────────
    // Pulse interval shrinks as fewer asteroids remain
    if (!g.asteroids.empty()) {
        float maxAsts  = (float)(2 + g.wave) * 4;
        float ratio    = std::min((float)g.asteroids.size() / maxAsts, 1.f);
        float interval = 0.22f + ratio * 0.58f;   // 0.22s (few) → 0.80s (many)
        g.beatTimer -= dt;
        if (g.beatTimer <= 0) {
            audio.playBeat(g.beatIndex);
            g.beatIndex = 1 - g.beatIndex;
            g.beatTimer = interval;
        }
    }
}

// ─── Render ───────────────────────────────────────────────────────────────────

static void render(SDL_Renderer* r, const Game& g, float totalTime, int fps) {
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderClear(r);

    SDL_Color white = {255, 255, 255, 255};
    SDL_Color cyan  = {0,   220, 255, 255};
    SDL_Color red   = {255,  80,  80, 255};
    SDL_Color grey  = {160, 160, 160, 255};

    for (const auto& a : g.asteroids)
        drawPoly(r, a.shape, a.pos, a.angle, white);

    for (const auto& b : g.bullets)
        drawCircle(r, b.pos, BULLET_RADIUS, cyan);

    const Ship& s = g.ship;
    if (s.alive) {
        bool visible = (s.respawnTimer <= 0) ||
                       (static_cast<int>(totalTime * 8) % 2 == 0);
        if (visible) {
            SDL_Color shipCol = (s.respawnTimer > 0) ? grey : white;
            drawPoly(r, Ship::shape(), s.pos, s.angle, shipCol);
            if (s.thrusting && (static_cast<int>(totalTime * 20) % 2) == 0)
                drawPoly(r, Ship::flameShape(), s.pos, s.angle, {255, 160, 0, 255});
        }
    }

    renderNumber(r, g.score, 16, 16, 2.5f, white);

    for (int i = 0; i < s.lives; ++i) {
        std::vector<Vec2> mini = {{0,-9},{7,7},{-7,7}};
        drawPoly(r, mini, {SCREEN_W - 30.f - i*28.f, 20.f}, 270.f, white);
    }

    if (g.state == GameState::GameOver) {
        const char* go = "GAME OVER";
        float scale = 4.f, charW = 8.f * scale;
        float startX = (SCREEN_W - strlen(go) * charW) / 2.f;
        for (size_t i = 0; i < strlen(go); ++i)
            renderChar(r, go[i], startX + i*charW, SCREEN_H/2.f - 40, scale, red);

        const char* sc = "SCORE";
        float scX = (SCREEN_W - 5 * 8.f * 2.5f) / 2.f - 20;
        for (size_t i = 0; i < strlen(sc); ++i)
            renderChar(r, sc[i], scX + i*8.f*2.5f, SCREEN_H/2.f + 20, 2.5f, white);
        renderNumber(r, g.score, scX + 5*8.f*2.5f + 12, SCREEN_H/2.f + 20, 2.5f, cyan);

        const char* pr = "PRESS R TO RESTART";
        float prX = (SCREEN_W - strlen(pr) * 8.f * 1.5f) / 2.f;
        for (size_t i = 0; i < strlen(pr); ++i)
            renderChar(r, pr[i], prX + i*8.f*1.5f, SCREEN_H/2.f + 70, 1.5f, grey);
    }

    if (g.state == GameState::NewWave && g.wave > 0) {
        SDL_Color waveCol = {100, 255, 140, 255};
        const char* wt = "WAVE";
        float wtX = (SCREEN_W - 4 * 8.f * 3.f) / 2.f - 30;
        for (size_t i = 0; i < strlen(wt); ++i)
            renderChar(r, wt[i], wtX + i*8.f*3.f, SCREEN_H/2.f - 30, 3.f, waveCol);
        renderNumber(r, g.wave, wtX + 4*8.f*3.f + 8, SCREEN_H/2.f - 30, 3.f, waveCol);
    }

    // ── FPS counter ───────────────────────────────────────────────────────
    renderNumber(r, fps, 16, SCREEN_H - 22, 1.2f, {80, 80, 80, 255});

    SDL_RenderPresent(r);
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main(int /*argc*/, char* /*argv*/[]) {
    srand(static_cast<unsigned>(time(nullptr)));

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window*   window   = SDL_CreateWindow("Asteroids", SCREEN_W, SCREEN_H, 0);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!window || !renderer) {
        SDL_Log("Window/Renderer creation failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_SetRenderVSync(renderer, 1);

    AudioSystem audio;
    audio.init();   // non-fatal — game runs silently if audio unavailable

    Game game;
    game.reset();

    bool   running    = true;
    float  totalTime  = 0;
    int    fps        = 0;
    int    fpsFrames  = 0;
    float  fpsAccum   = 0;
    Uint64 prev       = SDL_GetTicks();

    while (running) {
        Uint64 now = SDL_GetTicks();
        float  dt  = (now - prev) / 1000.f;
        prev       = now;
        if (dt > 0.05f) dt = 0.05f;
        totalTime += dt;
        fpsAccum  += dt;
        fpsFrames++;
        if (fpsAccum >= 0.5f) {
            fps       = (int)roundf(fpsFrames / fpsAccum);
            fpsFrames = 0;
            fpsAccum  = 0;
        }

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_EVENT_QUIT) running = false;
            if (ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) running = false;
                if (ev.key.key == SDLK_R && game.state == GameState::GameOver)
                    game.reset();
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        bool left   = keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A];
        bool right  = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
        bool thrust = keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W];
        bool shoot  = keys[SDL_SCANCODE_SPACE];

        update(game, dt, left, right, thrust, shoot, audio);
        audio.updateThrust(thrust && game.ship.alive);
        render(renderer, game, totalTime, fps);
    }

    audio.shutdown();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
