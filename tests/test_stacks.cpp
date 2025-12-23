#include "common.h"
#include "sgl_stack.h"
#include "treiber_stack.h"
#include "elimination_stack.h"
#include "flat_combining_stack.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <string>

// Single-thread basic LIFO correctness
template <typename Stack>
void single_thread_stack_check(const std::string& name)
{
    std::cout << "[Single-thread] Testing " << name << "...\n";

    Stack st;
    const int N = 5;

    for (int i = 1; i <= N; ++i)
        st.push(i);

    int x;
    for (int i = N; i >= 1; --i) {
        bool ok = st.pop(x);
        check(ok, (name + " single-thread: pop should succeed").c_str());
        check(x == i, (name + " single-thread: LIFO violated").c_str());
    }

    bool ok = st.pop(x);
    check(!ok, (name + " single-thread: extra pop should fail").c_str());

    std::cout << "  -> " << name << " single-thread OK\n\n";
}

// Multi-thread push, single-thread pop, identical for all stacks
template <typename Stack>
void multi_thread_stack_check(const std::string& name,
                              int threads,
                              int pushes_per_thread)
{
    std::cout << "[Multi-thread] Testing " << name
              << " with " << threads << " threads, "
              << pushes_per_thread << " pushes per thread...\n";

    Stack st;
    const int total_pushes = threads * pushes_per_thread;

    auto worker = [&](int id) {
        for (int i = 0; i < pushes_per_thread; ++i) {
            int val = id * pushes_per_thread + i;   // disjoint ranges
            st.push(val);
        }
    };

    std::vector<std::thread> ts;
    for (int t = 0; t < threads; ++t)
        ts.emplace_back(worker, t);
    for (auto& th : ts)
        th.join();

    std::cout << "  All pushes done. Popping...\n";

    std::vector<int> popped;
    popped.reserve(total_pushes);
    int x;

    while (st.pop(x)) {
        popped.push_back(x);
    }

    std::cout << "  pushed total: " << total_pushes << "\n";
    std::cout << "  popped total: " << popped.size() << "\n";

    check((int)popped.size() == total_pushes,
          (name + " multi-thread: count mismatch").c_str());

    std::sort(popped.begin(), popped.end());

    for (int id = 0; id < threads; ++id) {
        for (int i = 0; i < pushes_per_thread; ++i) {
            int expected = id * pushes_per_thread + i;
            bool found = std::binary_search(popped.begin(), popped.end(), expected);
            check(found,
                  (name + " multi-thread: missing value " +
                   std::to_string(expected)).c_str());
        }
    }

    std::cout << "  -> " << name << " multi-thread OK\n\n";
}

int main()
{
    std::cout << "===== Unified Stack Test (SGL, Treiber, Elimination, FlatCombining) =====\n\n";

    const int threads = 4;
    const int pushes_per_thread = 20000;

    // 1) Single-thread correctness for each
    single_thread_stack_check<SGLStack<int>>("SGLStack");
    single_thread_stack_check<TreiberStack<int>>("TreiberStack");
    single_thread_stack_check<EliminationStack<int>>("EliminationStack");
    single_thread_stack_check<FlatCombiningStack<int>>("FlatCombiningStack");

    // 2) Multi-thread identical workload for each
    multi_thread_stack_check<SGLStack<int>>("SGLStack", threads, pushes_per_thread);
    multi_thread_stack_check<TreiberStack<int>>("TreiberStack", threads, pushes_per_thread);
    multi_thread_stack_check<EliminationStack<int>>("EliminationStack", threads, pushes_per_thread);
    multi_thread_stack_check<FlatCombiningStack<int>>("FlatCombiningStack", threads, pushes_per_thread);

    std::cout << "===== test_stacks OK =====\n";
    return 0;
}
