#include "common.h"
#include "sgl_queue.h"
#include "ms_queue.h"
#include "flat_combining_queue.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <string>
#include <atomic>

// Single-thread FIFO check
template <typename Queue>
void single_thread_queue_check(const std::string& name)
{
    std::cout << "[Single-thread] Testing " << name << "...\n";

    Queue q;
    const int N = 5;

    for (int i = 1; i <= N; ++i)
        q.enqueue(i);

    int x;
    for (int i = 1; i <= N; ++i) {
        bool ok = q.dequeue(x);
        check(ok, (name + " single-thread: dequeue should succeed").c_str());
        check(x == i, (name + " single-thread: FIFO violated").c_str());
    }

    bool ok = q.dequeue(x);
    check(!ok, (name + " single-thread: extra dequeue should fail").c_str());

    std::cout << "  -> " << name << " single-thread OK\n\n";
}

// Multi-producer / single-consumer identical pattern for all queues
template <typename Queue>
void mpsc_queue_check(const std::string& name,
                      int producers,
                      int per_thread)
{
    std::cout << "[Multi-producer/Single-consumer] Testing " << name
              << " with " << producers << " producers, "
              << per_thread << " items each...\n";

    Queue q;
    const int total = producers * per_thread;
    std::atomic<int> produced{0};

    auto producer = [&](int id) {
        for (int i = 0; i < per_thread; ++i) {
            int val = id * per_thread + i;
            q.enqueue(val);
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> prod_threads;
    for (int p = 0; p < producers; ++p)
        prod_threads.emplace_back(producer, p);

    std::vector<int> out;
    out.reserve(total);

    std::thread consumer([&] {
        while ((int)out.size() < total) {
            int x;
            if (q.dequeue(x)) {
                out.push_back(x);
            } else {
                // if all produced and queue seems empty, let it exit
                if (produced.load(std::memory_order_relaxed) >= total && q.empty())
                    break;
                std::this_thread::yield();
            }
        }
    });

    for (auto& t : prod_threads)
        t.join();
    consumer.join();

    std::cout << "  produced: " << total << "\n";
    std::cout << "  consumed: " << out.size() << "\n";

    check((int)out.size() == total,
          (name + " MPSC: count mismatch").c_str());

    std::sort(out.begin(), out.end());
    for (int id = 0; id < producers; ++id) {
        for (int i = 0; i < per_thread; ++i) {
            int expected = id * per_thread + i;
            bool found = std::binary_search(out.begin(), out.end(), expected);
            check(found,
                  (name + " MPSC: missing value " +
                   std::to_string(expected)).c_str());
        }
    }

    std::cout << "  -> " << name << " MPSC test OK\n\n";
}
bool empty() const {
    std::lock_guard<std::mutex> lk(combine_m_);
    return data_.empty();
}
int main()
{
    std::cout << "===== Unified Queue Test (SGLQueue, MSQueue, FlatCombiningQueue) =====\n\n";

    const int producers = 4;
    const int per_thread = 25000;

    // 1) Single-thread FIFO correctness
    single_thread_queue_check<SGLQueue<int>>("SGLQueue");
    single_thread_queue_check<MSQueue<int>>("MSQueue");
    single_thread_queue_check<FlatCombiningQueue<int>>("FlatCombiningQueue");

    // 2) Multi-producer single-consumer identical workload
    mpsc_queue_check<SGLQueue<int>>("SGLQueue", producers, per_thread);
    mpsc_queue_check<MSQueue<int>>("MSQueue", producers, per_thread);
    mpsc_queue_check<FlatCombiningQueue<int>>("FlatCombiningQueue", producers, per_thread);

    std::cout << "===== test_queues OK =====\n";
    return 0;
}
