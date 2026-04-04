#include "audio_system.hpp"

#include "constants.hpp"
#include "math.hpp"

#include <algorithm>
#include <cmath>

static constexpr int AUDIO_SR    = 44100;
static constexpr int VOICE_COUNT = 8;

static std::vector<float> genShootSound() {
    int n = (int)(AUDIO_SR * 0.13f);
    std::vector<float> buf(n);
    float phase = 0;
    for (int i = 0; i < n; ++i) {
        float t   = i / (float)AUDIO_SR;
        float ft  = 750.f * expf(-t * 14.f) + 180.f;
        phase    += ft / AUDIO_SR;
        buf[i]    = sinf(2 * PI * phase) * expf(-t * 22.f) * 0.5f;
    }
    return buf;
}

static std::vector<float> genNoiseBurst(float dur, float lpAlpha, float decayK, float vol) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float s = 0;
    for (int i = 0; i < n; ++i) {
        float t = i / (float)AUDIO_SR;
        s       = s * (1.f - lpAlpha) + eng_randf(-1.f, 1.f) * lpAlpha;
        buf[i]  = s * expf(-t * decayK) * vol;
    }
    return buf;
}

static std::vector<float> genDeathSound() {
    int n = (int)(AUDIO_SR * 0.9f);
    std::vector<float> buf(n);
    float phase = 0, ns = 0;
    for (int i = 0; i < n; ++i) {
        float t   = i / (float)AUDIO_SR;
        float env = expf(-t * 2.8f);
        float freq = 380.f * expf(-t * 3.5f) + 55.f;
        phase    += freq / AUDIO_SR;
        ns        = ns * 0.86f + eng_randf(-1.f, 1.f) * 0.14f;
        buf[i]    = (sinf(2 * PI * phase) * 0.45f + ns * 0.55f) * env * 0.75f;
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
        buf[i]  = sinf(2 * PI * phase) * expf(-t * 20.f) * 0.45f;
    }
    return buf;
}

static std::vector<float> genThrustChunk(float dur) {
    int n = (int)(AUDIO_SR * dur);
    std::vector<float> buf(n);
    float s = 0;
    for (int i = 0; i < n; ++i) {
        float t    = i / (float)AUDIO_SR;
        s          = s * 0.87f + eng_randf(-1.f, 1.f) * 0.13f;
        float fade = std::min(t / 0.008f, std::min((dur - t) / 0.008f, 1.f));
        buf[i]     = s * fade * 0.28f;
    }
    return buf;
}

bool AudioSystem::init() {
    spec = {SDL_AUDIO_F32, 1, AUDIO_SR};
    dev  = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!dev) {
        SDL_Log("Audio open failed: %s", SDL_GetError());
        return false;
    }
    for (int i = 0; i < VOICE_COUNT; ++i) {
        voices[i] = SDL_CreateAudioStream(&spec, &spec);
        SDL_BindAudioStream(dev, voices[i]);
    }
    thrustVoice = SDL_CreateAudioStream(&spec, &spec);
    SDL_BindAudioStream(dev, thrustVoice);
    sndShoot     = genShootSound();
    sndExpLarge  = genNoiseBurst(0.55f, 0.04f, 5.f, 0.85f);
    sndExpMedium = genNoiseBurst(0.35f, 0.12f, 8.f, 0.75f);
    sndExpSmall  = genNoiseBurst(0.20f, 0.35f, 14.f, 0.65f);
    sndDeath     = genDeathSound();
    sndBeat[0]   = genBeat(120.f, 0.07f);
    sndBeat[1]   = genBeat(90.f, 0.07f);
    return true;
}

void AudioSystem::shutdown() {
    for (int i = 0; i < VOICE_COUNT; ++i)
        if (voices[i])
            SDL_DestroyAudioStream(voices[i]);
    if (thrustVoice)
        SDL_DestroyAudioStream(thrustVoice);
    if (dev)
        SDL_CloseAudioDevice(dev);
}

void AudioSystem::play(const std::vector<float>& samples) {
    if (!dev || samples.empty())
        return;
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

void AudioSystem::updateThrust(bool on) {
    if (!dev || !on)
        return;
    int threshold = (int)(AUDIO_SR * 0.08f * sizeof(float));
    if (SDL_GetAudioStreamQueued(thrustVoice) < threshold) {
        auto chunk = genThrustChunk(0.12f);
        SDL_PutAudioStreamData(thrustVoice, chunk.data(),
                               (int)(chunk.size() * sizeof(float)));
    }
}
