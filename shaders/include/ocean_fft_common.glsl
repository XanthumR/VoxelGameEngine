// Shared by ocean_spectrum.comp and ocean_fft.comp (OceanSimulation, src/Rendering/OceanSimulation.cpp).
//
// FFT ocean (Tessendorf):
//   ocean_spectrum.comp - evolves the initial spectrum h0(k) to time t and builds the spectra
//                         of every field the renderer needs, packed two real fields per complex
//   ocean_fft.comp      - one radix-2 inverse FFT along each row (or column) of every cascade,
//                         in shared memory; run once horizontally then once vertically
//
// Packed spectra (each RGBA holds two complex numbers, X + iY for real fields X and Y):
//   A = (Dx + i Dz, h + i dDx/dz)        B = (dh/dx + i dh/dz, dDx/dx + i dDz/dz)
// After both FFT passes the real and imaginary parts are exactly those fields:
//   A = (Dx, Dz, h, dDx/dz)              B = (dh/dx, dh/dz, dDx/dx, dDz/dz)
// D is the horizontal (choppy) displacement; h the height; all in voxels.

#define N 256
#define LOG_N 8
const float PI = 3.14159265358979;
const float GRAVITY = 9.81; // 1 voxel = 1 m

vec2 cmul(vec2 a, vec2 b) { return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x); }
