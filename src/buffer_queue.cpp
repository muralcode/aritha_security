// Author: Lerato Mokoena
// Company: Arithaoptix pty Ltd.

#include <atomic>
#include <vector>
#include <optional>

// A lock ring buffer for concurrency
template <typename T, size_t Capacity>
class BufferQueue {
public:
    BufferQueue()
        : m_head(0)
        , m_tail(0) 
    {
        m_buffer.resize(Capacity);
    }

    bool push(const T& item) {
        auto currentTail = m_tail.load(std::memory_order_relaxed);
        auto nextTail = (currentTail + 1) % Capacity;
        if (nextTail == m_head.load(std::memory_order_acquire)) {
            // If Full
            return false;
        }
        m_buffer[currentTail] = item;
        m_tail.store(nextTail, std::memory_order_release);
        return true;
    }

    bool push(T&& item) {
        auto currentTail = m_tail.load(std::memory_order_relaxed);
        auto nextTail = (currentTail + 1) % Capacity;
        if (nextTail == m_head.load(std::memory_order_acquire)) {
            // It's Full
            return false;
        }
        m_buffer[currentTail] = std::move(item);
        m_tail.store(nextTail, std::memory_order_release);
        return true;
    }

    std::optional<T> pop() {
        auto currentHead = m_head.load(std::memory_order_relaxed);
        if (currentHead == m_tail.load(std::memory_order_acquire)) {
            // It's Empty
            return std::nullopt;
        }
        T item = std::move(m_buffer[currentHead]);
        m_head.store((currentHead + 1) % Capacity, std::memory_order_release);
        return item;
    }

private:
    std::vector<T> m_buffer;
    std::atomic<size_t> m_head;
    std::atomic<size_t> m_tail;
};
