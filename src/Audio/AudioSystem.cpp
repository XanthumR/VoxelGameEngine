#include "Audio/AudioSystem.h"

#include "miniaudio.h"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace {

constexpr std::array<const char*, (size_t)Sfx::Count> SFX_FILES = { "click", "place", "demolish", "road", "notify", "coins", "upgrade", "error" };
constexpr int VOICES = 4;          // Of each one-shot
constexpr float EASE_SECONDS = 0.6f; // Ambience volumes follow the levels this fast

} // namespace

struct AudioSystem::Impl {
    ma_engine engine{};
    bool running = false;
    std::array<ma_sound, AMBIENCE_COUNT> loops{};
    std::array<bool, AMBIENCE_COUNT> loopLoaded{};
    std::array<float, AMBIENCE_COUNT> volumes{};
    std::array<std::array<ma_sound, VOICES>, (size_t)Sfx::Count> voices{};
    std::array<std::array<bool, VOICES>, (size_t)Sfx::Count> voiceLoaded{};
    std::array<int, (size_t)Sfx::Count> nextVoice{};

    ~Impl() {
        if (!running) return;
        for (int i = 0; i < AMBIENCE_COUNT; i++) {
            if (loopLoaded[i]) ma_sound_uninit(&loops[i]);
        }
        for (size_t s = 0; s < voices.size(); s++) {
            for (int v = 0; v < VOICES; v++) {
                if (voiceLoaded[s][v]) ma_sound_uninit(&voices[s][v]);
            }
        }
        ma_engine_uninit(&engine);
    }
};

AudioSystem::AudioSystem() : m_Impl(std::make_unique<Impl>()) {}
AudioSystem::~AudioSystem() = default;

bool AudioSystem::Init(const std::string& directory) {
    Impl& audio = *m_Impl;
    if (ma_engine_init(nullptr, &audio.engine) != MA_SUCCESS) {
        std::cerr << "No audio device: the game runs without sound." << std::endl;
        return false;
    }
    audio.running = true;
    for (int i = 0; i < AMBIENCE_COUNT; i++) {
        std::string path = directory + "/" + AMBIENCE_FILES[i] + ".wav";
        audio.loopLoaded[i] = ma_sound_init_from_file(&audio.engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &audio.loops[i]) == MA_SUCCESS;
        if (!audio.loopLoaded[i]) {
            std::cerr << "Missing sound " << path << std::endl;
            continue;
        }
        ma_sound_set_looping(&audio.loops[i], MA_TRUE);
        ma_sound_set_volume(&audio.loops[i], 0.0f);
        // Each loop starts at another point, so loops of the same length do not beat together
        ma_uint64 frames = 0;
        ma_sound_get_length_in_pcm_frames(&audio.loops[i], &frames);
        if (frames > 0) ma_sound_seek_to_pcm_frame(&audio.loops[i], frames * (ma_uint64)i / AMBIENCE_COUNT);
        ma_sound_start(&audio.loops[i]);
    }
    for (size_t s = 0; s < SFX_FILES.size(); s++) {
        std::string path = directory + "/" + SFX_FILES[s] + ".wav";
        for (int v = 0; v < VOICES; v++) {
            audio.voiceLoaded[s][v] = ma_sound_init_from_file(&audio.engine, path.c_str(), MA_SOUND_FLAG_DECODE, nullptr, nullptr, &audio.voices[s][v]) == MA_SUCCESS;
        }
        if (!audio.voiceLoaded[s][0]) std::cerr << "Missing sound " << path << std::endl;
    }
    return true;
}

void AudioSystem::SetAmbience(const std::array<float, AMBIENCE_COUNT>& levels, float deltaTime) {
    Impl& audio = *m_Impl;
    if (!audio.running) return;
    float ease = 1.0f - std::exp(-deltaTime / EASE_SECONDS);
    for (int i = 0; i < AMBIENCE_COUNT; i++) {
        if (!audio.loopLoaded[i]) continue;
        audio.volumes[i] += (levels[i] - audio.volumes[i]) * ease;
        ma_sound_set_volume(&audio.loops[i], audio.volumes[i]);
    }
}

void AudioSystem::Play(Sfx sound) {
    Impl& audio = *m_Impl;
    if (!audio.running) return;
    size_t s = (size_t)sound;
    int v = audio.nextVoice[s];
    audio.nextVoice[s] = (v + 1) % VOICES;
    if (!audio.voiceLoaded[s][v]) return;
    ma_sound_seek_to_pcm_frame(&audio.voices[s][v], 0);
    ma_sound_start(&audio.voices[s][v]);
}

void AudioSystem::SetVolume(float volume) {
    if (m_Impl->running) ma_engine_set_volume(&m_Impl->engine, std::clamp(volume, 0.0f, 1.0f));
}
