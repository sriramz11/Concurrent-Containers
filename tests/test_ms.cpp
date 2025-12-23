#include "ms_queue.h"
#include "common.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <atomic>

int main()
{
    std::cout << "===== test_ms: Lock-free Michael–Scott Queue =====\n\n";

    // ---------- Single-thread basic test ----------
    {
        std::cout << "[MSQueue] Basic single-thread test...\n";
        MSQueue<int> q;
        check(q.empty(), "MSQueue should start empty");

        q.enqueue(1);
        q.enqueue(2);
        q.enqueue(3);

        int x;
        check(q.dequeue(x) && x == 1, "MSQueue FIFO 1");
        check(q.dequeue(x) && x == 2, "MSQueue FIFO 2");
        check(q.dequeue(x) && x == 3, "MSQueue FIFO 3");
        check(!q.dequeue(x), "MSQueue empty dequeue");
        std::cout << "[MSQueue] Basic test passed.\n\n";
    }

    // ---------- Single-producer / single-consumer ----------
    {
        std::cout << "[MSQueue] Single-producer / single-consumer test...\n";
        MSQueue<int> q;
        const int N = 50000;

        std::thread producer([&] {
            for (int i = 0; i < N; ++i) {
                q.enqueue(i);
            }
        });

        std::vector<int> out;
        out.reserve(N);

        std::thread consumer([&] {
            int x;
            while ((int)out.size() < N) {
                if (q.dequeue(x)) {
                    out.push_back(x);
                } else {
                    std::this_thread::yield();
                }
            }
        });

        producer.join();
        consumer.join();

        std::cout << "  produced: " << N << "\n";
        std::cout << "  consumed: " << out.size() << "\n";
        check((int)out.size() == N, "MSQueue SPSC count match");

        for (int i = 0; i < N; ++i) {
            check(out[i] == i, "MSQueue SPSC FIFO order");
        }
        std::cout << "[MSQueue] SPSC test passed.\n\n";
    }

    // ---------- Multi-producer / single-consumer ----------
    {
        std::cout << "[MSQueue] Multi-producer / single-consumer test...\n";
        MSQueue<int> q;
        const int threads = 4;
        const int per_thread = 25000;
        const int total = threads * per_thread;

        std::atomic<int> produced{0};

        auto producer = [&](int id) {
            for (int i = 0; i < per_thread; ++i) {
                int val = id * per_thread + i;
                q.enqueue(val);
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        };

        std::vector<std::thread> producers;
        for (int t = 0; t < threads; ++t)
            producers.emplace_back(producer, t);

        std::vector<int> out;
        out.reserve(total);

        std::thread consumer([&] {
            while ((int)out.size() < total) {
                int x;
                if (q.dequeue(x)) {
                    out.push_back(x);
                } else {
                    if (produced.load(std::memory_order_relaxed) >= total && q.empty())
                        break;
                    std::this_thread::yield();
                }
            }
        });

        for (auto& p : producers)
            p.join();
        consumer.join();

        std::cout << "  produced: " << total << "\n";
        std::cout << "  consumed: " << out.size() << "\n";
        check((int)out.size() == total, "MSQueue MPSC count match");

        std::sort(out.begin(), out.end());
        for (int id = 0; id < threads; ++id) {
            for (int i = 0; i < per_thread; ++i) {
                int expected = id * per_thread + i;
                bool found = std::binary_search(out.begin(), out.end(), expected);
                check(found, "MSQueue MPSC missing element");
            }
        }

        std::cout << "[MSQueue] MPSC test passed.\n\n";
    }

    std::cout << "===== test_ms OK =====\n";
    return 0;
}
