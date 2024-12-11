#pragma once

#include <SDL3/SDL.h>
#include <vector>
#include <optional>

struct Audio {
    SDL_AudioStream *stream = nullptr;
    SDL_AudioSpec spec{};
    std::vector<uint8_t> data;
};

std::optional<Audio> load_ogg(const char *path, float volume=1.0f);
std::optional<Audio> load_wav(const char *path, float volume=1.0f);
