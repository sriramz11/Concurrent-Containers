#include "common.h"
#include "cv_nospurious.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// Small helper
static void print_header(const char* name)
{
    std::cout << "=============================\n";
    std::cout << name << "\n";
    std::cout << "=============================\n";
}

// ------------------------------------------------------------------
// Test 1: single waiter, small number of notify_one() calls.
// We expect exactly one wake per notify_one.
// ------------------------------------------------------------------
static void test_single_waiter()
{
    print_header("[test_single_waiter]");

    CVNoSpurious cv;
    std::mutex   m;

    const int rounds = 10;   // small & fast
    int        wake_count = 0;

    std::atomic<bool> ready{false};

    std::thread worker([&] {
        std::unique_lock<std::mutex> lk(m);

        ready.store(true, std::memory_order_release);

        for (int i = 0; i < rounds; ++i) {
            cv.wait(lk);          // no-predicate version
            ++wake_count;
            std::cout << "  [worker] woke " << (i + 1) << "/" << rounds << "\n";
        }
    });

    // Wait until worker holds the lock and is ready to wait
    while (!ready.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }

    // Give worker a moment to actually enter wait() the first time
    std::this_thread::sleep_for(5ms);

    // Main thread: do 'rounds' notify_one() calls
    for (int i = 0; i < rounds; ++i) {
        {
            std::lock_guard<std::mutex> lk(m);
            // In a real program you would update shared state here.
        }
        std::cout << "  [main] notify_one #" << (i + 1) << "\n";
        cv.notify_one();
        std::this_thread::sleep_for(2ms); // small pause for visibility
    }

    worker.join();

    std::cout << "  expected wakes = " << rounds
              << ", actual wakes = " << wake_count << "\n";

    check(wake_count == rounds,
          "single_waiter: wake_count must equal number of notify_one() calls");

    std::cout << "[test_single_waiter] OK\n\n";
}

// ------------------------------------------------------------------
// Test 2: multiple waiters, notify_all().
// Each notify_all should wake all waiters once.
// ------------------------------------------------------------------
static void test_broadcast_many_waiters()
{
    print_header("[test_broadcast_many_waiters]");

    CVNoSpurious cv;
    std::mutex   m;

    const int threads = 3;   // small
    const int rounds  = 5;   // small

    std::vector<int> wakes(threads, 0);
    std::atomic<int> ready{0};

    std::vector<std::thread> workers;
    workers.reserve(threads);

    for (int id = 0; id < threads; ++id) {
        workers.emplace_back([&, id] {
            std::unique_lock<std::mutex> lk(m);

            ready.fetch_add(1, std::memory_order_release);

            for (int r = 0; r < rounds; ++r) {
                cv.wait(lk);   // no-predicate version
                ++wakes[id];
                std::cout << "  [worker " << id << "] woke "
                          << (r + 1) << "/" << rounds << "\n";
            }
        });
    }

    // Wait until all threads are in their loops and hold the lock at least once
    while (ready.load(std::memory_order_acquire) < threads) {
        std::this_thread::yield();
    }

    // Give them a brief moment to enter the first wait()
    std::this_thread::sleep_for(5ms);

    for (int r = 0; r < rounds; ++r) {
        {
            std::lock_guard<std::mutex> lk(m);
            // In real code, update condition here.
        }
        std::cout << "  [main] notify_all #" << (r + 1) << "\n";
        cv.notify_all();
        std::this_thread::sleep_for(5ms);
    }

    for (auto& t : workers) {
        t.join();
    }

    for (int id = 0; id < threads; ++id) {
        std::cout << "  thread " << id << " wakes = " << wakes[id] << "\n";
        check(wakes[id] == rounds,
              "broadcast: each waiter must wake once per notify_all()");
    }

    std::cout << "[test_broadcast_many_waiters] OK\n\n";
}

int main()
{
    test_single_waiter();
    test_broadcast_many_waiters();

    std::cout << "test_cv OK\n";
    return 0;
}
