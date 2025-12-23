#include "common.h"

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

struct Options {
    std::string kind;
    std::string algo;
    int threads = 4;
    std::size_t total_ops = 200000;
};

static bool parse_args(int argc, char** argv, Options& opt)
{
    for (int i = 1; i < argc; ++i) {
        std::string a(argv[i]);

        if (a.rfind("--kind=", 0) == 0) { opt.kind = a.substr(7); continue; }
        if (a.rfind("--algo=", 0) == 0) { opt.algo = a.substr(7); continue; }
        if (a.rfind("--threads=", 0) == 0) { opt.threads = std::stoi(a.substr(10)); continue; }
        if (a.rfind("--ops=", 0) == 0) { opt.total_ops = std::stoull(a.substr(6)); continue; }

        std::cerr << "Unknown argument: " << a << "\n";
        return false;
    }

    return true;
}

// ---------------- STACK ----------------

template<typename Stack>
void run_stack(const std::string& name, const Options& opt)
{
    using clock = std::chrono::steady_clock;

    Stack st;
    const int th = opt.threads;
    const std::size_t total = opt.total_ops;
    const std::size_t per = total / th;

    std::atomic<std::size_t> pushed{0};

    auto worker = [&]() {
        for (std::size_t i = 0; i < per; ++i) {
            st.push((int)i);
            pushed.fetch_add(1, std::memory_order_relaxed);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(th);

    auto t0 = clock::now();
    for (int i = 0; i < th; ++i) threads.emplace_back(worker);
    for (auto& t : threads) t.join();
    auto t1 = clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::size_t popcnt = 0;
    int x;
    while (st.pop(x)) popcnt++;

    std::cout << "=== STACK RUN ===\n";
    std::cout << "algo=" << name << "\n";
    std::cout << "threads=" << th << "\n";
    std::cout << "pushed=" << pushed << "\n";
    std::cout << "popped=" << popcnt << "\n";
    std::cout << "time_ms=" << ms << "\n";
    std::cout << "==========\n";
}

// ---------------- QUEUE ----------------

template<typename Queue>
void run_queue(const std::string& name, const Options& opt)
{
    using clock = std::chrono::steady_clock;

    Queue q;
    const int prod = opt.threads;
    const std::size_t total = opt.total_ops;
    const std::size_t per = total / prod;

    std::atomic<std::size_t> produced{0};
    std::atomic<std::size_t> consumed{0};

    auto p = [&]() {
        for (std::size_t i = 0; i < per; ++i) {
            q.enqueue((int)i);
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto c = [&]() {
        int x;
        while (true) {
            if (q.dequeue(x)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            if (produced.load() >= total) {
                while (q.dequeue(x)) consumed++;
                break;
            }
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(prod);

    auto t0 = clock::now();
    for (int i = 0; i < prod; ++i) threads.emplace_back(p);
    std::thread consumer(c);
    for (auto& t : threads) t.join();
    consumer.join();
    auto t1 = clock::now();

    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

    std::cout << "=== QUEUE RUN ===\n";
    std::cout << "algo=" << name << "\n";
    std::cout << "producers=" << prod << "\n";
    std::cout << "produced=" << produced << "\n";
    std::cout << "consumed=" << consumed << "\n";
    std::cout << "time_ms=" << ms << "\n";
    std::cout << "==========\n";
}

// ---------------- MAIN ----------------

int main(int argc, char** argv)
{
    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Bad args\n";
        return 1;
    }

    if (opt.kind == "stack") {
        if (opt.algo == "sgl")
            run_stack<SGLStack<int>>("SGLStack", opt);
        else if (opt.algo == "treiber")
            run_stack<TreiberStack<int>>("TreiberStack", opt);
        else if (opt.algo == "elim")
            run_stack<EliminationStack<int>>("EliminationStack", opt);
        else if (opt.algo == "fc")
            run_stack<FlatCombiningStack<int>>("FlatCombiningStack", opt);
        else {
            std::cerr << "Unknown stack algo\n";
            return 1;
        }
    }
    else if (opt.kind == "queue") {
        if (opt.algo == "sgl")
            run_queue<SGLQueue<int>>("SGLQueue", opt);
        else if (opt.algo == "ms")
            run_queue<MSQueue<int>>("MSQueue", opt);
        else if (opt.algo == "fc")
            run_queue<FlatCombiningQueue<int>>("FlatCombiningQueue", opt);
        else {
            std::cerr << "Unknown queue algo\n";
            return 1;
        }
    }
    else {
        std::cerr << "Unknown kind\n";
        return 1;
    }

    return 0;
}
