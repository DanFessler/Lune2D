#pragma once

#include <SDL3/SDL.h>

#include <vector>

struct AudioSystem {
    SDL_AudioDeviceID dev             = 0;
    SDL_AudioSpec     spec            = {};
    SDL_AudioStream*  voices[8]     = {};
    SDL_AudioStream*  thrustVoice   = nullptr;

    std::vector<float> sndShoot, sndExpLarge, sndExpMedium, sndExpSmall, sndDeath;
    std::vector<float> sndBeat[2];

    bool init();
    void shutdown();
    void play(const std::vector<float>& samples);
    void updateThrust(bool on);
};
