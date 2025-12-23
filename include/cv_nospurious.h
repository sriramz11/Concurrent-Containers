#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

class CVNoSpurious {
public:
    CVNoSpurious() : seq_(0) {}

    // Same names as std::condition_variable
    void notify_one()
    {
        // Bump generation so any waiter that started before
        // sees a change and can return from wait().
        seq_.fetch_add(1, std::memory_order_release);
        cv_.notify_one();
    }

    void notify_all()
    {
        seq_.fetch_add(1, std::memory_order_release);
        cv_.notify_all();
    }

    // -------- wait without predicate: we remove spurious wakeups --------
    void wait(std::unique_lock<std::mutex>& lock)
    {
        // Record current generation when we go to sleep
        std::size_t my_seq = seq_.load(std::memory_order_acquire);

        // Internally use the predicate form so spurious wakeups are hidden.
        cv_.wait(lock, [&] {
            // We only return when generation has advanced.
            return seq_.load(std::memory_order_acquire) != my_seq;
        });
    }

    // -------- wait with predicate: just forward to std::cv --------
    // The usual "wait(lock, pred)" pattern already masks spurious wakeups
    // because pred() must be true on return.
    template <class Predicate>
    void wait(std::unique_lock<std::mutex>& lock, Predicate pred)
    {
        cv_.wait(lock, pred);
    }

private:
    std::condition_variable   cv_;
    std::atomic<std::size_t>  seq_;
};
