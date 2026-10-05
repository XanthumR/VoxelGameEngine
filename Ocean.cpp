#include "Ocean.h"

#include <cmath>
#include <random>
#include <vector>

namespace {

constexpr float PI = 3.14159265358979f;
constexpr float GRAVITY = 9.81f;          // 1 voxel = 1 m
constexpr float SMALL_WAVE_CUTOFF = 0.5f; // Voxels; waves much shorter than this are damped away

GLuint CreateArrayTexture(int size, int layers, GLenum filter, GLenum wrap) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
    glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA32F, size, size, layers);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, wrap);
    return texture;
}

} // namespace

void OceanSimulation::Init(GLuint spectrumProgram, GLuint fftProgram) {
    m_SpectrumProgram = spectrumProgram;
    m_FftProgram = fftProgram;
    m_TimeLocation = glGetUniformLocation(spectrumProgram, "time");
    m_TileSizesLocation = glGetUniformLocation(spectrumProgram, "tileSizes");
    m_HorizontalLocation = glGetUniformLocation(fftProgram, "horizontal");
    m_FinalPassLocation = glGetUniformLocation(fftProgram, "finalPass");

    glActiveTexture(GL_TEXTURE0);
    m_InitialSpectrum = CreateArrayTexture(GRID, CASCADES, GL_NEAREST, GL_REPEAT);
    m_SpectrumA = CreateArrayTexture(GRID, CASCADES, GL_NEAREST, GL_REPEAT);
    m_SpectrumB = CreateArrayTexture(GRID, CASCADES, GL_NEAREST, GL_REPEAT);
    m_TempA = CreateArrayTexture(GRID, CASCADES, GL_NEAREST, GL_REPEAT);
    m_TempB = CreateArrayTexture(GRID, CASCADES, GL_NEAREST, GL_REPEAT);
    m_OutputA = CreateArrayTexture(GRID, CASCADES, GL_LINEAR, GL_REPEAT); // Sampled by the renderer
    m_OutputB = CreateArrayTexture(GRID, CASCADES, GL_LINEAR, GL_REPEAT);

    BuildInitialSpectrum();
}

// Phillips spectrum, split into bands so each wavelength lives in exactly one cascade, with
// random Gaussian amplitudes. Stores h0(k) and conj(h0(-k)) per texel.
void OceanSimulation::BuildInitialSpectrum() {
    // Cascade c keeps |k| in [bandStart[c], bandStart[c + 1]): each cascade takes over from its
    // 4th harmonic, where its tile resolves the waves well
    const float bandStart[CASCADES + 1] = {
        0.0f,
        2.0f * PI * 4.0f / m_TileSizes[1],
        2.0f * PI * 4.0f / m_TileSizes[2],
        1e30f,
    };
    const float largestWave = m_WindSpeed * m_WindSpeed / GRAVITY;

    auto phillips = [&](glm::vec2 k, int cascade) {
        float kLength = glm::length(k);
        if (kLength < 1e-6f || kLength < bandStart[cascade] || kLength >= bandStart[cascade + 1]) return 0.0f;
        float alignment = glm::dot(k / kLength, m_WindDirection);
        float spectrum = std::exp(-1.0f / (kLength * largestWave * kLength * largestWave)) / (kLength * kLength * kLength * kLength);
        spectrum *= alignment * alignment;
        if (alignment < 0.0f) spectrum *= 0.1f; // Little energy travelling against the wind
        spectrum *= std::exp(-kLength * kLength * SMALL_WAVE_CUTOFF * SMALL_WAVE_CUTOFF);
        return spectrum;
    };

    auto waveVector = [&](int n, int m, int cascade) {
        return 2.0f * PI * glm::vec2((float)(n - GRID / 2), (float)(m - GRID / 2)) / m_TileSizes[cascade];
    };

    // Scale the spectrum so the summed height has the target standard deviation:
    // for the unnormalized inverse FFT used here, variance = sum of P(k)
    double totalEnergy = 0.0;
    for (int c = 0; c < CASCADES; c++)
        for (int m = 0; m < GRID; m++)
            for (int n = 0; n < GRID; n++)
                totalEnergy += phillips(waveVector(n, m, c), c);
    float scale = totalEnergy > 0.0 ? (float)(m_HeightStdDev * m_HeightStdDev / totalEnergy) : 0.0f;

    std::mt19937 rng(20261005); // Fixed seed: the same sea every run
    std::normal_distribution<float> gaussian(0.0f, 1.0f);

    std::vector<glm::vec2> h0((size_t)GRID * GRID * CASCADES);
    for (int c = 0; c < CASCADES; c++) {
        for (int m = 0; m < GRID; m++) {
            for (int n = 0; n < GRID; n++) {
                // E|h0|^2 = P/2, so h(k,t) = h0(k) e^iwt + conj(h0(-k)) e^-iwt has E|h|^2 = P
                float amplitude = std::sqrt(phillips(waveVector(n, m, c), c) * scale * 0.25f);
                float re = gaussian(rng), im = gaussian(rng);
                h0[((size_t)c * GRID + m) * GRID + n] = glm::vec2(re, im) * amplitude;
            }
        }
    }

    std::vector<glm::vec4> texels(h0.size());
    for (int c = 0; c < CASCADES; c++) {
        for (int m = 0; m < GRID; m++) {
            for (int n = 0; n < GRID; n++) {
                // Index of -k in the centered layout
                int nm = (GRID - n) % GRID, mm = (GRID - m) % GRID;
                glm::vec2 a = h0[((size_t)c * GRID + m) * GRID + n];
                glm::vec2 b = h0[((size_t)c * GRID + mm) * GRID + nm];
                texels[((size_t)c * GRID + m) * GRID + n] = glm::vec4(a.x, a.y, b.x, -b.y);
            }
        }
    }

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_InitialSpectrum);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, GRID, GRID, CASCADES, GL_RGBA, GL_FLOAT, texels.data());
}

void OceanSimulation::Update(float time) {
    // 1. Evolve the spectrum to this time
    glUseProgram(m_SpectrumProgram);
    glUniform1f(m_TimeLocation, time);
    glUniform3fv(m_TileSizesLocation, 1, &m_TileSizes[0]);
    glBindImageTexture(0, m_InitialSpectrum, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, m_SpectrumA, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(3, m_SpectrumB, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glDispatchCompute(GRID / 16, GRID / 16, CASCADES);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    // 2. Inverse FFT along rows, then columns
    glUseProgram(m_FftProgram);
    glUniform1i(m_HorizontalLocation, 1);
    glUniform1i(m_FinalPassLocation, 0);
    glBindImageTexture(0, m_SpectrumA, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, m_SpectrumB, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, m_TempA, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(3, m_TempB, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glDispatchCompute(GRID, CASCADES, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    glUniform1i(m_HorizontalLocation, 0);
    glUniform1i(m_FinalPassLocation, 1);
    glBindImageTexture(0, m_TempA, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(1, m_TempB, 0, GL_TRUE, 0, GL_READ_ONLY, GL_RGBA32F);
    glBindImageTexture(2, m_OutputA, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glBindImageTexture(3, m_OutputB, 0, GL_TRUE, 0, GL_WRITE_ONLY, GL_RGBA32F);
    glDispatchCompute(GRID, CASCADES, 1);
    glMemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT);
}
