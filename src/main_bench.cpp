#include "common.h"
#include "sgl_stack.h"
#include "treiber_stack.h"
#include "elimination_stack.h"
#include "flat_combining_stack.h"

#include "sgl_queue.h"
#include "ms_queue.h"
#include "flat_combining_queue.h"

#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <iostream>
#include <iomanip>
#include <atomic>

// Small helper for timing
using clock_type = std::chrono::high_resolution_clock;

struct BenchResult {
    std::string kind;        // "stack" or "queue"
    std::string name;        // data structure name
    int         threads;     // stacks: threads, queues: producers
    std::size_t requested_ops;      // what we asked for
    std::size_t actual_ops;         // what we actually executed
    double      time_ms;            // wall time in ms
    double      ops_per_sec;        // throughput
};

// ---------------------------------------------------------------------------
// Stack benchmark: constant total pushes, varying threads
// ---------------------------------------------------------------------------
//
// Pattern:
//   - One shared stack.
//   - Total requested pushes = TOTAL_OPS (constant).
//   - For a given T, each thread pushes floor(TOTAL_OPS / T) items.
//   - After threads finish, main thread pops everything (not included in ops).
//   - We measure time for the push phase only, and compute throughput.
// ---------------------------------------------------------------------------

template <typename Stack>
BenchResult bench_stack_const_total(const std::string& name,
                                    int threads,
                                    std::size_t total_pushes_requested)
{
    Stack s;

    std::size_t per_thread = total_pushes_requested / threads;
    std::size_t actual_pushes = per_thread * threads;

    std::cout << "\n[STACK BENCH] " << name << "\n"
              << "  threads                : " << threads << "\n"
              << "  requested total pushes : " << total_pushes_requested << "\n"
              << "  per-thread pushes      : " << per_thread << "\n"
              << "  actual total pushes    : " << actual_pushes << "\n";

    auto worker = [&](int id) {
        // Just generate some simple values
        for (std::size_t i = 0; i < per_thread; ++i) {
            int v = id * static_cast<int>(per_thread) + static_cast<int>(i);
            s.push(v);
        }
    };

    std::vector<std::thread> ts;
    ts.reserve(threads);

    auto t_start = clock_type::now();
    for (int t = 0; t < threads; ++t)
        ts.emplace_back(worker, t);
    for (auto& th : ts)
        th.join();
    auto t_end = clock_type::now();

    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    double time_ms = static_cast<double>(dur_ns) / 1e6;
    double time_s  = static_cast<double>(dur_ns) / 1e9;
    double throughput = (time_s > 0.0) ? (static_cast<double>(actual_pushes) / time_s) : 0.0;

    std::cout << "  time (ms)              : " << time_ms << "\n"
              << "  pushes/sec             : " << throughput << "\n";

    // Optional sanity check: pop everything
    std::size_t popped = 0;
    int x = 0;
    while (s.pop(x)) {
        ++popped;
    }
    std::cout << "  sanity: popped count   : " << popped << "\n";
    check(popped == actual_pushes, (name + " popped != pushed").c_str());

    BenchResult res;
    res.kind         = "stack";
    res.name         = name;
    res.threads      = threads;
    res.requested_ops= total_pushes_requested;
    res.actual_ops   = actual_pushes;
    res.time_ms      = time_ms;
    res.ops_per_sec  = throughput;
    return res;
}

// ---------------------------------------------------------------------------
// Queue benchmark: constant total items, varying producer count (MPSC)
// ---------------------------------------------------------------------------
//
// Pattern:
//   - P producers, 1 consumer (MPSC).
//   - Total requested items = TOTAL_ITEMS (constant).
//   - Each producer enqueues floor(TOTAL_ITEMS / P) items.
//   - Consumer dequeues until it has seen all enqueued items.
//   - We measure time from start of producers+consumer to their completion.
//   - Throughput is computed on total enqueues + dequeues (2 * actual_items).
// ---------------------------------------------------------------------------

template <typename Queue>
BenchResult bench_queue_const_total(const std::string& name,
                                    int producers,
                                    std::size_t total_items_requested)
{
    Queue q;

    std::size_t per_producer = total_items_requested / producers;
    std::size_t actual_items = per_producer * producers;

    std::cout << "\n[QUEUE BENCH] " << name << "\n"
              << "  producers              : " << producers << "\n"
              << "  requested total items  : " << total_items_requested << "\n"
              << "  per-producer items     : " << per_producer << "\n"
              << "  actual total items     : " << actual_items << "\n";

    std::atomic<std::size_t> produced{0};
    std::atomic<std::size_t> consumed{0};

    auto producer_fn = [&](int id) {
        for (std::size_t i = 0; i < per_producer; ++i) {
            int v = id * static_cast<int>(per_producer) + static_cast<int>(i);
            q.enqueue(v);
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> prod_threads;
    prod_threads.reserve(producers);

    auto consumer_fn = [&]() {
        std::size_t local_count = 0;
        int v = 0;
        while (local_count < actual_items) {
            if (q.dequeue(v)) {
                ++local_count;
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                // queue empty, but either producers still going or minor race
                if (produced.load(std::memory_order_relaxed) >= actual_items) {
                    // If producers are done and queue is empty, break
                    if (q.empty())
                        break;
                }
                std::this_thread::yield();
            }
        }
    };

    auto t_start = clock_type::now();

    // start consumer
    std::thread consumer(consumer_fn);
    // start producers
    for (int p = 0; p < producers; ++p)
        prod_threads.emplace_back(producer_fn, p);

    for (auto& t : prod_threads)
        t.join();
    consumer.join();

    auto t_end = clock_type::now();

    auto dur_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t_end - t_start).count();
    double time_ms = static_cast<double>(dur_ns) / 1e6;
    double time_s  = static_cast<double>(dur_ns) / 1e9;

    std::size_t enq = produced.load(std::memory_order_relaxed);
    std::size_t deq = consumed.load(std::memory_order_relaxed);
    std::size_t logical_ops = enq + deq;

    double throughput = (time_s > 0.0) ? (static_cast<double>(logical_ops) / time_s) : 0.0;

    std::cout << "  time (ms)              : " << time_ms << "\n"
              << "  enqueued               : " << enq << "\n"
              << "  dequeued               : " << deq << "\n"
              << "  logical ops (enq+deq)  : " << logical_ops << "\n"
              << "  logical ops/sec        : " << throughput << "\n";

    check(enq == actual_items, (name + " enq != actual_items").c_str());
    check(deq == actual_items, (name + " deq != actual_items").c_str());

    BenchResult res;
    res.kind         = "queue";
    res.name         = name;
    res.threads      = producers;          // here 'threads' means producer count
    res.requested_ops= total_items_requested;
    res.actual_ops   = logical_ops;        // counting enq+deq as ops
    res.time_ms      = time_ms;
    res.ops_per_sec  = throughput;
    return res;
}

// ---------------------------------------------------------------------------
// Pretty-print a CSV-like summary line
// ---------------------------------------------------------------------------
void print_summary_header()
{
    std::cout << "\n===== SUMMARY (CSV-like) =====\n";
    std::cout << "kind,name,threads,requested_ops,actual_ops,time_ms,ops_per_sec\n";
}

void print_summary_line(const BenchResult& r)
{
    std::cout << r.kind << ","
              << r.name << ","
              << r.threads << ","
              << r.requested_ops << ","
              << r.actual_ops << ","
              << std::fixed << std::setprecision(3) << r.time_ms << ","
              << std::fixed << std::setprecision(0) << r.ops_per_sec
              << "\n";
}

// ---------------------------------------------------------------------------
// main
//
// Usage:
//   ./bench
//       -> uses default thread counts {1,2,4,8,16} and total_ops = 200000
//
//   ./bench <threads>
//       -> single thread count, default total_ops = 200000
//
//   ./bench <threads> <total_ops>
//       -> single thread count, custom total_ops
//
// Notes:
//   - For stacks: <threads> is #threads.
//   - For queues: <threads> is #producers (1 consumer).
// ---------------------------------------------------------------------------
int main(int argc, char** argv)
{
    // Default configuration
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    std::size_t total_stack_pushes = 200000;
    std::size_t total_queue_items  = 200000;

    if (argc >= 2) {
        // if user passes a thread count, only use that
        int t = std::stoi(argv[1]);
        if (t <= 0) {
            std::cerr << "Invalid thread count " << t << ", must be > 0\n";
            return 1;
        }
        thread_counts.clear();
        thread_counts.push_back(t);
    }
    if (argc >= 3) {
        std::size_t ops = static_cast<std::size_t>(std::stoll(argv[2]));
        if (ops == 0) {
            std::cerr << "Invalid ops " << ops << ", must be > 0\n";
            return 1;
        }
        total_stack_pushes = ops;
        total_queue_items  = ops;
    }

    std::cout << "===== Concurrent Containers Benchmark =====\n";
    std::cout << "Constant total workload mode.\n";
    std::cout << "Stack total pushes (per run) : " << total_stack_pushes << "\n";
    std::cout << "Queue total items  (per run) : " << total_queue_items  << "\n";
    std::cout << "Thread counts                 : ";
    for (auto t : thread_counts) std::cout << t << " ";
    std::cout << "\n";

    std::vector<BenchResult> all_results;

    // ------------------------
    // Stack benchmarks
    // ------------------------
    std::cout << "\n========== STACKS ==========\n";
    for (int t : thread_counts) {
        all_results.push_back(
            bench_stack_const_total<SGLStack<int>>("SGLStack", t, total_stack_pushes));
        all_results.push_back(
            bench_stack_const_total<TreiberStack<int>>("TreiberStack", t, total_stack_pushes));
        all_results.push_back(
            bench_stack_const_total<EliminationStack<int>>("EliminationStack", t, total_stack_pushes));
        all_results.push_back(
            bench_stack_const_total<FlatCombiningStack<int>>("FlatCombiningStack", t, total_stack_pushes));
    }

    // ------------------------
    // Queue benchmarks
    // ------------------------
    std::cout << "\n========== QUEUES ==========\n";
    for (int producers : thread_counts) {
        all_results.push_back(
            bench_queue_const_total<SGLQueue<int>>("SGLQueue", producers, total_queue_items));
        all_results.push_back(
            bench_queue_const_total<MSQueue<int>>("MSQueue", producers, total_queue_items));
        all_results.push_back(
            bench_queue_const_total<FlatCombiningQueue<int>>("FlatCombiningQueue", producers, total_queue_items));
    }

    // ------------------------
    // Summary
    // ------------------------
    print_summary_header();
    for (const auto& r : all_results) {
        print_summary_line(r);
    }

    std::cout << "\nDone.\n";
    return 0;
}
