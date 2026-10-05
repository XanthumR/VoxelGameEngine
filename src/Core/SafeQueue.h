#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>

// Thread-safe FIFO queue, used between the main thread and the chunk generation workers
template <typename T>
class SafeQueue {
public:
    void Push(T value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Queue.push(std::move(value));
        m_Condition.notify_one();
    }

    // Returns false immediately if the queue is empty
    bool TryPop(T& value) {
        std::lock_guard<std::mutex> lock(m_Mutex);
        if (m_Queue.empty()) return false;
        value = std::move(m_Queue.front());
        m_Queue.pop();
        return true;
    }

    // Blocks until an item is available
    void WaitAndPop(T& value) {
        std::unique_lock<std::mutex> lock(m_Mutex);
        m_Condition.wait(lock, [this]() { return !m_Queue.empty(); });
        value = std::move(m_Queue.front());
        m_Queue.pop();
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(m_Mutex);
        return m_Queue.size();
    }

private:
    std::queue<T> m_Queue;
    std::mutex m_Mutex;
    std::condition_variable m_Condition;
};
