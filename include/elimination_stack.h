#pragma once

#include "common.h"
#include <atomic>
#include <array>
#include <random>
#include <thread>

// A Treiber stack with an elimination array.
// - Fast path: plain Treiber push/pop.
// - Slow path: if we see repeated CAS failures, we try to
//   "eliminate" with an opposing pop/push in the arena.

template <typename T>
class EliminationStack {
private:
    // ---------------- Central Treiber stack ----------------
    struct Node {
        T     value;
        Node* next;
        explicit Node(const T& v) : value(v), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};

    // ---------------- Elimination arena ----------------
    static constexpr int ELIM_ARRAY_SIZE  = 16;
    static constexpr int ELIM_TRIES       = 4;   // how many slots to try
    static constexpr int CAS_THRESHOLD    = 4;   // after this many CAS failures, try elimination
    static constexpr int SPIN_ITERS       = 10;  // how long to wait for a match

    // Each slot either holds nullptr or a Node* offered by a pusher.
    std::array<std::atomic<Node*>, ELIM_ARRAY_SIZE> arena_;

    // Thread-local RNG for picking slots
    static thread_local std::mt19937 rng_;

    static int random_slot()
    {
        std::uniform_int_distribution<int> dist(0, ELIM_ARRAY_SIZE - 1);
        return dist(rng_);
    }

    // Try to match this push with a pop in the arena.
    // Returns true if elimination succeeded (node consumed by a pop),
    // false if we should fall back to Treiber.
    bool try_elim_push(Node*& n)
    {
        for (int attempt = 0; attempt < ELIM_TRIES; ++attempt) {
            int   idx      = random_slot();
            Node* expected = nullptr;

            // Offer our node into an empty slot
            if (arena_[idx].compare_exchange_strong(
                    expected, n,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed)) {

                // Wait briefly for a pop to take it
                for (int i = 0; i < SPIN_ITERS; ++i) {
                    Node* cur = arena_[idx].load(std::memory_order_acquire);
                    if (cur != n) {
                        // Someone took it (slot changed or cleared)
                        n = nullptr;  // pop now owns and will delete
                        return true;
                    }
                    std::this_thread::yield();
                }

                // Timed out; try to reclaim the slot
                Node* cur = n;
                if (arena_[idx].compare_exchange_strong(
                        cur, nullptr,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed)) {
                    // We removed our own node: no elimination, keep n
                    return false;
                } else {
                    // A pop took it while we were trying to clear
                    n = nullptr;
                    return true;
                }
            }
        }
        return false;
    }

    // Try to satisfy a pop from the arena instead of the central stack.
    bool try_elim_pop(T& out)
    {
        for (int attempt = 0; attempt < ELIM_TRIES; ++attempt) {
            int   idx = random_slot();
            Node* n   = arena_[idx].exchange(nullptr, std::memory_order_acq_rel);
            if (n != nullptr) {
                out = n->value;
                delete n;
                return true;
            }
        }
        return false;
    }

public:
    EliminationStack()
    {
        for (auto& slot : arena_) {
            slot.store(nullptr, std::memory_order_relaxed);
        }
    }

    ~EliminationStack()
    {
        // Drain central stack
        Node* n = head_.load(std::memory_order_relaxed);
        while (n) {
            Node* next = n->next;
            delete n;
            n = next;
        }
        // Drain any leftover nodes in arena (best-effort)
        for (auto& slot : arena_) {
            Node* p = slot.exchange(nullptr, std::memory_order_relaxed);
            while (p) {
                Node* next = p->next;
                delete p;
                p = next;
            }
        }
    }

    bool empty()
    {
        if (head_.load(std::memory_order_acquire) != nullptr)
            return false;
        for (auto& slot : arena_) {
            if (slot.load(std::memory_order_acquire) != nullptr)
                return false;
        }
        return true;
    }

    // ---------------- push with Treiber + elimination ----------------
    void push(const T& v)
    {
        Node* n = new Node(v);
        int   cas_failures = 0;

        while (true) {
            Node* old_head = head_.load(std::memory_order_relaxed);
            n->next        = old_head;

            if (head_.compare_exchange_weak(
                    old_head, n,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                // Treiber fast path succeeded
                return;
            }

            // CAS failed: possible contention
            if (++cas_failures >= CAS_THRESHOLD) {
                if (try_elim_push(n)) {
                    // Eliminated with a pop; node consumed
                    return;
                }
                cas_failures = 0; // try CAS again
            }
        }
    }

    // ---------------- pop with Treiber + elimination ----------------
    bool pop(T& out)
    {
        int cas_failures = 0;

        while (true) {
            Node* old_head = head_.load(std::memory_order_acquire);

            if (!old_head) {
                // Stack appears empty: try elimination before returning false
                if (try_elim_pop(out))
                    return true;
                return false;
            }

            Node* next = old_head->next;

            if (head_.compare_exchange_weak(
                    old_head, next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                // Treiber pop succeeded
                out = old_head->value;
                delete old_head;
                return true;
            }

            // CAS failed: possible contention
            if (++cas_failures >= CAS_THRESHOLD) {
                if (try_elim_pop(out))
                    return true;
                cas_failures = 0;
            }
        }
    }
};

// Define the thread-local RNG
template <typename T>
thread_local std::mt19937 EliminationStack<T>::rng_{std::random_device{}()};
