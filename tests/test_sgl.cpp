#include "sgl_stack.h"
#include "sgl_queue.h"
#include "common.h"

#include <iostream>
#include <thread>
#include <vector>
#include <algorithm>

int main()
{
    std::cout << "===== test_sgl: Single Global Lock Stack & Queue =====\n\n";

    // ---------- SGLStack basic test ----------
    {
        std::cout << "[SGLStack] Basic single-thread test...\n";
        SGLStack<int> st;
        check(st.empty(), "SGLStack should start empty");

        st.push(1);
        st.push(2);
        st.push(3);

        int x;
        check(st.pop(x) && x == 3, "SGLStack LIFO 3");
        check(st.pop(x) && x == 2, "SGLStack LIFO 2");
        check(st.pop(x) && x == 1, "SGLStack LIFO 1");
        check(!st.pop(x), "SGLStack empty pop");
        std::cout << "[SGLStack] Basic test passed.\n\n";
    }

    // ---------- SGLQueue basic test ----------
    {
        std::cout << "[SGLQueue] Basic single-thread test...\n";
        SGLQueue<int> q;
        check(q.empty(), "SGLQueue should start empty");

        q.enqueue(10);
        q.enqueue(20);
        q.enqueue(30);

        int x;
        check(q.dequeue(x) && x == 10, "SGLQueue FIFO 10");
        check(q.dequeue(x) && x == 20, "SGLQueue FIFO 20");
        check(q.dequeue(x) && x == 30, "SGLQueue FIFO 30");
        check(!q.dequeue(x), "SGLQueue empty dequeue");
        std::cout << "[SGLQueue] Basic test passed.\n\n";
    }

    // ---------- SGLStack multi-thread push/pop ----------
    {
        std::cout << "[SGLStack] Multi-thread push/pop test...\n";
        const int threads = 4;
        const int pushes_per_thread = 10000;
        const int total_pushes = threads * pushes_per_thread;

        SGLStack<int> st;

        auto worker = [&](int id) {
            for (int i = 0; i < pushes_per_thread; ++i) {
                st.push(id * pushes_per_thread + i);
            }
        };

        std::vector<std::thread> ts;
        for (int t = 0; t < threads; ++t)
            ts.emplace_back(worker, t);
        for (auto& th : ts)
            th.join();

        std::cout << "[SGLStack] Finished pushes. Now popping...\n";

        std::vector<int> popped;
        popped.reserve(total_pushes);
        int x;
        while (st.pop(x)) {
            popped.push_back(x);
        }

        std::cout << "  pushed total: " << total_pushes << "\n";
        std::cout << "  popped total: " << popped.size() << "\n";

        check((int)popped.size() == total_pushes, "SGLStack multi-thread count match");

        // check that all values are unique and in expected range
        std::sort(popped.begin(), popped.end());
        for (int id = 0; id < threads; ++id) {
            for (int i = 0; i < pushes_per_thread; ++i) {
                int expected = id * pushes_per_thread + i;
                check(std::binary_search(popped.begin(), popped.end(), expected),
                      "SGLStack missing element in multi-thread test");
            }
        }

        std::cout << "[SGLStack] Multi-thread test passed.\n\n";
    }

    std::cout << "===== test_sgl OK =====\n";
    return 0;
}
