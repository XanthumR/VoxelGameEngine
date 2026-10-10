#pragma once

#include <glad/glad.h>

#include <array>

// GPU time per section of a frame, for the F3 window. Mark("name") ends the running section and
// starts the next; each mark is a timestamp query read back FRAMES frames later, so the CPU never
// waits for the GPU. Times are smoothed over about half a second.
class GpuTimers {
public:
    static constexpr int MAX_SECTIONS = 16;
    static constexpr int FRAMES = 4; // Frames in flight before a frame's queries are read

    void Init();
    void BeginFrame(); // Reads the oldest frame's results; then marks start a new frame
    void Mark(const char* name);
    void EndFrame(); // Ends the last section

    int Count() const { return m_Count; }
    const char* Name(int section) const { return m_Names[section]; }
    float Milliseconds(int section) const { return m_Milliseconds[section]; }
    float TotalMilliseconds() const;

private:
    struct Frame {
        std::array<GLuint, MAX_SECTIONS + 1> queries = {};
        std::array<const char*, MAX_SECTIONS> names = {};
        int marks = 0;
    };
    std::array<Frame, FRAMES> m_Frames;
    int m_Current = 0;
    bool m_Ready = false;

    // Smoothed results of the frames read back
    std::array<const char*, MAX_SECTIONS> m_Names = {};
    std::array<float, MAX_SECTIONS> m_Milliseconds = {};
    int m_Count = 0;
};
