#pragma once

// Rendering options the player can change at runtime (debug overlay, hotkeys, launch options)
struct RenderSettings {
    static constexpr float MIN_RENDER_SCALE = 0.5f;

    float renderScale = 1.0f;     // Fraction of the window resolution the rays are traced at
    bool halfResShadows = true;   // Soft shadows once per 2x2 pixel quad instead of per pixel
    bool chunkViewer = false;     // Draw chunk edges (disables empty-space skipping)
    bool lightVisualizer = false; // False-color light levels
};
