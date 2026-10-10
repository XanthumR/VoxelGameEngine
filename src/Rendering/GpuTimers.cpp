#include "Rendering/GpuTimers.h"

#include <cstdint>
#include <cstring>

void GpuTimers::Init() {
    for (Frame& frame : m_Frames) glGenQueries((GLsizei)frame.queries.size(), frame.queries.data());
    m_Ready = true;
}

void GpuTimers::BeginFrame() {
    if (!m_Ready) return;
    m_Current = (m_Current + 1) % FRAMES;
    Frame& frame = m_Frames[m_Current];

    // This slot was last written FRAMES frames ago: its timestamps are (almost always) in
    if (frame.marks > 1) {
        GLint available = 0;
        glGetQueryObjectiv(frame.queries[frame.marks - 1], GL_QUERY_RESULT_AVAILABLE, &available);
        if (available) {
            uint64_t previous = 0;
            glGetQueryObjectui64v(frame.queries[0], GL_QUERY_RESULT, &previous);
            int sections = frame.marks - 1;
            bool sameSections = sections == m_Count;
            for (int i = 0; i < sections && sameSections; i++) sameSections = std::strcmp(m_Names[i], frame.names[i]) == 0;
            for (int i = 0; i < sections; i++) {
                uint64_t time = 0;
                glGetQueryObjectui64v(frame.queries[i + 1], GL_QUERY_RESULT, &time);
                float ms = (float)(time - previous) / 1.0e6f;
                previous = time;
                // Smoothed, unless the sections changed (camera mode, objects on or off)
                m_Milliseconds[i] = sameSections ? m_Milliseconds[i] * 0.95f + ms * 0.05f : ms;
                m_Names[i] = frame.names[i];
            }
            m_Count = sections;
        }
    }
    frame.marks = 0;
}

void GpuTimers::Mark(const char* name) {
    if (!m_Ready) return;
    Frame& frame = m_Frames[m_Current];
    if (frame.marks >= MAX_SECTIONS) return;
    glQueryCounter(frame.queries[frame.marks], GL_TIMESTAMP);
    frame.names[frame.marks] = name;
    frame.marks++;
}

void GpuTimers::EndFrame() {
    if (!m_Ready) return;
    Frame& frame = m_Frames[m_Current];
    if (frame.marks == 0 || frame.marks > MAX_SECTIONS) return;
    glQueryCounter(frame.queries[frame.marks], GL_TIMESTAMP);
    frame.marks++; // The last mark only ends the section before it
}

float GpuTimers::TotalMilliseconds() const {
    float total = 0.0f;
    for (int i = 0; i < m_Count; i++) total += m_Milliseconds[i];
    return total;
}
