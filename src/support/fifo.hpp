#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

template <typename T, size_t Size>
class Fifo {
  public:
    Fifo() = default;
    ~Fifo() { clear(); }

    // Disable copies
    Fifo(const Fifo<T, Size>&) = delete;
    Fifo& operator=(const Fifo<T, Size>&) = delete;
    // Disable moves
    Fifo(Fifo<T, Size>&& other) = delete;
    Fifo&& operator=(Fifo<T, Size>&& other) = delete;

    void clear() {
        while (!m_queue.empty()) m_queue.pop();
    }

    T& front() { return m_queue.front(); }

    T pop() {
        T val = T();
        if (!m_queue.empty()) {
            val = m_queue.front();
            m_queue.pop();
        }
        return val;
    }

    T peek() {
        T val = T();
        if (!m_queue.empty()) {
            val = m_queue.front();
        }
        return val;
    }

    void push(T val) {
        if (m_queue.size() >= Size) {
            m_queue.back() = val;
        } else {
            m_queue.push(val);
        }
    }

    template <size_t size>
    void push_bulk(std::array<T, size> values) {
        for (T& val : values) {
            push(val);
        }
    }

    size_t size() { return m_queue.size(); }

    bool empty() { return m_queue.empty(); }

  private:
    std::queue<T> m_queue;
};

template <typename T>
class TSFifo {
  public:
    TSFifo() = default;

    // Disable copies
    TSFifo(const TSFifo<T>&) = delete;
    TSFifo& operator=(const TSFifo<T>&) = delete;

    // Disable moves
    TSFifo(TSFifo<T>&& other) = delete;
    TSFifo&& operator=(TSFifo<T>&& other) = delete;

    virtual ~TSFifo() {}

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.empty();
    }

    T pop() {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_cv.wait(lock, [&] { return !m_queue.empty(); });

        T item = std::move(m_queue.front());
        m_queue.pop();
        return item;
    }

    //    void push(const T& item) {
    //        std::lock_guard<std::mutex> lock(m_mutex);
    //        m_queue.push(item);
    //        m_cv.notify_one();
    //    }

    void push(T&& item) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push(std::forward<T>(item));
        m_cv.notify_one();
    }

  private:
    std::queue<T> m_queue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
};
