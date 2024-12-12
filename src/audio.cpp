#include "audio.hpp"
#include "log.hpp"
#include "stb_vorbis.hpp"

#include <cstdlib>

std::optional<Audio> load_ogg(const char *path, float volume) {
    // NOTE: Can't use fopen on files inside an Android APK.
    // SDL provides IO abstraction for this.
    size_t data_size;
    uint8_t *data = static_cast<uint8_t*>(SDL_LoadFile(path, &data_size));

    if (!data) {
        LOG("Failed to open file '%s'.", path);
        return {};
    }

    Audio ret;

    short *output;
    int samples = stb_vorbis_decode_memory(data, data_size, &ret.spec.channels, &ret.spec.freq, &output);
  
    ret.data.resize(samples * ret.spec.channels * sizeof(short));
    memcpy(ret.data.data(), output, ret.data.size());

    free(output);
    SDL_free(data);

    ret.spec.format = SDL_AUDIO_S16LE;

    ret.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &ret.spec, NULL, NULL);

    if (!ret.stream) {
        LOG("Couldn't create audio stream: %s", SDL_GetError());
        return {};
    }


    if (volume > 0.0f && volume < 1.0f) {
        std::vector<uint8_t> tmp(ret.data.size());
        SDL_MixAudio(tmp.data(), ret.data.data(), ret.spec.format, ret.data.size(), volume);
        ret.data.swap(tmp);
    }

    return ret;
}

std::optional<Audio> load_wav(const char *path, float volume) {
    Audio ret;

    uint8_t *data = nullptr;
    uint32_t data_len;

    if (!SDL_LoadWAV(path, &ret.spec, &data, &data_len)) {
        LOG("Couldn't load .wav file: %s", path);
        return {};
    }

    ret.data.assign(data, data + data_len);
    SDL_free(data);

    ret.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &ret.spec, NULL, NULL);

    if (!ret.stream) {
        LOG("Couldn't create audio stream: %s", SDL_GetError());
        return {};
    }

    if (volume > 0.0f && volume < 1.0f) {
        std::vector<uint8_t> tmp(ret.data.size());
        SDL_MixAudio(tmp.data(), ret.data.data(), ret.spec.format, ret.data.size(), volume);
        ret.data.swap(tmp);
    }

    return ret;
}

