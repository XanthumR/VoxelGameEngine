#pragma once

#include "Gameplay/SoundScape.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string>

// One-shot sounds (assets/sounds/<name>.wav, cut from CC0 recordings by tools/sounds/fetch.py)
enum class Sfx : uint8_t { Click, Place, Demolish, Road, Notify, Coins, Upgrade, Error, Count };

// The game's sound, through miniaudio: the ambience loops (SoundScape), always playing, their
// volumes eased toward the levels asked for, and one-shot sounds, a few voices each so quick
// repeats overlap. Without an audio device (or the sound files) it stays silent and the game runs
// on.
class AudioSystem {
public:
    AudioSystem();
    ~AudioSystem();

    bool Init(const std::string& directory);
    void SetAmbience(const std::array<float, AMBIENCE_COUNT>& levels, float deltaTime);
    void Play(Sfx sound);
    void SetVolume(float volume); // 0..1, everything

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl; // Keeps miniaudio out of the header
};
