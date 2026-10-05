#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

// FFT ocean (Tessendorf). A wind-driven wave spectrum is built once on the CPU; every frame
// shaders/water/ocean_spectrum.comp evolves it in time and ocean_fft.comp inverse-FFTs it on the
// GPU into displacement, height and slope maps. Three cascades with unrelated tile sizes are
// summed so the tiling never lines up.
class OceanSimulation {
public:
    static constexpr int GRID = 256;    // FFT size per cascade (must match N in ocean_fft_common.glsl)
    static constexpr int CASCADES = 3;

    // Loads the shaders and allocates everything up front. Returns false if a shader failed.
    bool Init();

    // Runs the spectrum update and both FFT passes for this time (seconds)
    void Update(float time);

    // 2D array textures (one layer per cascade), linear + repeat, sampled at worldXZ / tile size
    GLuint DisplacementTexture() const { return m_OutputA; } // Dx, Dz, height, dDx/dz
    GLuint SlopeTexture() const { return m_OutputB; }        // dh/dx, dh/dz, dDx/dx, dDz/dz
    glm::vec3 TileSizes() const { return m_TileSizes; }
    float Choppiness() const { return m_Choppiness; } // Horizontal displacement strength, used for foam

private:
    void BuildInitialSpectrum();

    glm::vec3 m_TileSizes = glm::vec3(1031.0f, 257.0f, 67.0f); // Voxels; deliberately unrelated
    glm::vec2 m_WindDirection = glm::normalize(glm::vec2(1.0f, 0.35f));
    float m_WindSpeed = 10.0f;      // m/s; peak wavelength ~90 voxels
    float m_HeightStdDev = 0.7f;    // Target standard deviation of the summed height, voxels
    float m_Choppiness = 1.6f;

    GLuint m_SpectrumProgram = 0;
    GLuint m_FftProgram = 0;
    GLint m_TimeLocation = -1;
    GLint m_TileSizesLocation = -1;
    GLint m_HorizontalLocation = -1;
    GLint m_FinalPassLocation = -1;

    GLuint m_InitialSpectrum = 0;
    GLuint m_SpectrumA = 0, m_SpectrumB = 0; // Time-evolved spectra (FFT input)
    GLuint m_TempA = 0, m_TempB = 0;         // After the row pass
    GLuint m_OutputA = 0, m_OutputB = 0;     // Final spatial fields
};
