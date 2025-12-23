#include "treiber_stack.h"
#include "common.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>
#include <cstdint>

int main()
{
    std::cout << "===== test_treiber: Lock-free TreiberStack =====\n\n";

    // ---------- Single-thread basic test ----------
    {
        std::cout << "[TreiberStack] Basic single-thread test...\n";
        TreiberStack<int> st;
        check(st.empty(), "TreiberStack should start empty");

        st.push(1);
        st.push(2);
        st.push(3);

        int x;
        check(st.pop(x) && x == 3, "TreiberStack LIFO 3");
        check(st.pop(x) && x == 2, "TreiberStack LIFO 2");
        check(st.pop(x) && x == 1, "TreiberStack LIFO 1");
        check(!st.pop(x), "TreiberStack empty pop");
        std::cout << "[TreiberStack] Basic test passed.\n\n";
    }

    // ---------- Multi-thread push test ----------
    {
        std::cout << "[TreiberStack] Multi-thread push test (pop single-thread)...\n";
        const int threads = 4;
        const int pushes_per_thread = 20000;
        const int total_pushes = threads * pushes_per_thread;

        TreiberStack<std::uint64_t> st;

        auto worker = [&](int id) {
            for (int i = 0; i < pushes_per_thread; ++i) {
                std::uint64_t val = (static_cast<std::uint64_t>(id) << 32) | static_cast<std::uint32_t>(i);
                st.push(val);
            }
        };

        std::vector<std::thread> ts;
        for (int t = 0; t < threads; ++t)
            ts.emplace_back(worker, t);
        for (auto& th : ts)
            th.join();

        std::cout << "[TreiberStack] Finished pushes. Now popping...\n";

        std::vector<std::uint64_t> popped;
        popped.reserve(total_pushes);
        std::uint64_t v;
        while (st.pop(v)) {
            popped.push_back(v);
        }

        std::cout << "  pushed total: " << total_pushes << "\n";
        std::cout << "  popped total: " << popped.size() << "\n";

        check((int)popped.size() == total_pushes, "TreiberStack multi-thread count match");

        std::sort(popped.begin(), popped.end());

        for (int id = 0; id < threads; ++id) {
            for (int i = 0; i < pushes_per_thread; ++i) {
                std::uint64_t expected =
                    (static_cast<std::uint64_t>(id) << 32) | static_cast<std::uint32_t>(i);
                bool found = std::binary_search(popped.begin(), popped.end(), expected);
                check(found, "TreiberStack missing element in multi-thread test");
            }
        }

        std::cout << "[TreiberStack] Multi-thread push test passed.\n\n";
    }

    std::cout << "===== test_treiber OK =====\n";
    return 0;
}
