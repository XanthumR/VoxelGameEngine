// miniaudio's implementation, compiled once (src/Audio/AudioSystem.cpp uses it). Only WAV is needed.
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_GENERATION
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
