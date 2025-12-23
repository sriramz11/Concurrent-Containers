#pragma once

#include "common.h"
#include "hazard_pointers.h"
#include <optional>

template <typename T>
class MSQueue {
private:
    struct Node {
        std::atomic<Node*> next;
        std::optional<T>   value;

        Node() : next(nullptr), value(std::nullopt) {}        // dummy
        explicit Node(const T& v) : next(nullptr), value(v) {} // real
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    MSQueue()
    {
        Node* dummy = new Node(); // sentinel node
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~MSQueue()
    {
        // Drain nodes (single-threaded, so no hazard pointers needed)
        Node* n = head_.load(std::memory_order_relaxed);
        while (n) {
            Node* next = n->next.load(std::memory_order_relaxed);
            delete n;
            n = next;
        }
    }

    void enqueue(const T& value)
    {
        Node* new_node = new Node(value);

        while (true) {
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);

            if (tail == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // Tail is the real last node
                    if (tail->next.compare_exchange_weak(
                            next,
                            new_node,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        // Try to swing tail to new node (best-effort)
                        tail_.compare_exchange_strong(
                            tail,
                            new_node,
                            std::memory_order_acq_rel,
                            std::memory_order_relaxed);
                        return;
                    }
                } else {
                    // Tail is behind, advance it
                    tail_.compare_exchange_strong(
                        tail,
                        next,
                        std::memory_order_acq_rel,
                        std::memory_order_relaxed);
                }
            }
        }
    }

    bool dequeue(T& out)
    {
        hp::HazardPointerOwner hp_head;

        while (true) {
            Node* head = head_.load(std::memory_order_acquire);
            hp_head.set(head);

            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);

            if (head != head_.load(std::memory_order_acquire))
                continue; // changed, retry

            if (next == nullptr) {
                // Queue is empty
                return false;
            }

            if (head == tail) {
                // Tail is falling behind, advance it
                tail_.compare_exchange_strong(
                    tail,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_relaxed);
                continue;
            } else {
                // There is real data in next
                if (head_.compare_exchange_strong(
                        head,
                        next,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    // We moved head forward, safe to read next->value
                    out = *(next->value);
                    hp_head.clear();
                    // retire old head
                    hp::retired_list<Node>().retire(head);
                    return true;
                }
            }
        }
    }

    bool empty() const
    {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return next == nullptr;
    }
};
