#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <shared_mutex>
#include <unordered_set>

// The roots of trees that have been cut down, shared with the chunk worker threads so chunks
// generated later leave those trees out. Written by the simulation (main thread), read by the
// terrain generators (any thread).
class FelledTrees {
public:
    void Add(glm::ivec2 root) {
        std::unique_lock lock(m_Mutex);
        m_Roots.insert(Key(root));
    }
    void Remove(glm::ivec2 root) {
        std::unique_lock lock(m_Mutex);
        m_Roots.erase(Key(root));
    }
    bool Contains(glm::ivec2 root) const {
        std::shared_lock lock(m_Mutex);
        return m_Roots.count(Key(root)) != 0;
    }

    static uint64_t Key(glm::ivec2 root) { return ((uint64_t)(uint32_t)root.x << 32) | (uint32_t)root.y; }

private:
    mutable std::shared_mutex m_Mutex;
    std::unordered_set<uint64_t> m_Roots;
};
