#pragma once

#include "Simulation/GameObjects.h"

#include <glm/glm.hpp>

#include <array>
#include <cstdint>

// The ambience: how loud each looping sound should be, from where the camera looks and how far out
// it is. The sea and the wind are always there (the wind more the higher the camera); every other
// loop comes from the buildings in use near the camera's focus (a sawmill, a smithy, the town
// round the houses and the marketplace), each fading with its distance, summed and softly capped,
// so a whole quarter of houses is louder than one but never too loud. Zoomed out, the buildings'
// sounds blend into a quieter murmur.
enum class Ambience : uint8_t { Sea, Wind, Saw, Smithy, Mill, Animals, Town, Harbor, Count };
constexpr int AMBIENCE_COUNT = (int)Ambience::Count;
constexpr std::array<const char*, AMBIENCE_COUNT> AMBIENCE_FILES = { "sea", "wind", "saw", "smithy", "mill", "animals", "town", "harbor" };

// The loop a building makes while in use, Count for none
Ambience AmbienceOf(uint16_t buildingType);

// Volumes 0..1 per Ambience. focusColumn: the camera's focus (world columns); cameraDistance: voxels
// from the focus (the zoom)
std::array<float, AMBIENCE_COUNT> AmbienceLevels(const GameObjectRegistry& objects, glm::vec2 focusColumn, float cameraDistance);
