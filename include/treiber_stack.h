#pragma once

#include "common.h"
#include "hazard_pointers.h"

template <typename T>
class TreiberStack {
private:
    struct Node {
        T     value;
        Node* next;
        explicit Node(const T& v) : value(v), next(nullptr) {}
    };

    std::atomic<Node*> head_{nullptr};

public:
    TreiberStack() = default;

    ~TreiberStack()
    {
        // Drain remaining nodes without hazard logic (single-threaded now)
        Node* n = head_.load(std::memory_order_relaxed);
        while (n) {
            Node* next = n->next;
            delete n;
            n = next;
        }
    }

    void push(const T& value)
    {
        Node* new_node = new Node(value);
        new_node->next = nullptr;

        Node* old_head = head_.load(std::memory_order_relaxed);
        do {
            new_node->next = old_head;
        } while (!head_.compare_exchange_weak(
            old_head,
            new_node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }

    bool pop(T& out)
    {
        hp::HazardPointerOwner hpo;
        Node*                  old_head = nullptr;

        while (true) {
            old_head = head_.load(std::memory_order_acquire);
            if (!old_head) {
                hpo.clear();
                return false;
            }
            hpo.set(old_head); // protect old_head

            // Re-read to ensure consistency
            if (old_head != head_.load(std::memory_order_acquire))
                continue;

            Node* next = old_head->next;
            if (head_.compare_exchange_weak(
                    old_head,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                break; // success
            }
        }

        out = old_head->value;
        hpo.clear();
        hp::retired_list<Node>().retire(old_head);
        return true;
    }

    bool empty() const
    {
        return head_.load(std::memory_order_acquire) == nullptr;
    }
};
