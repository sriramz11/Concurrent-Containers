#include "flat_combining_stack.h"
#include "flat_combining_queue.h"
#include "common.h"
#include <iostream>

int main()
{
    // ---- Test FlatCombiningStack ----
    {
        FlatCombiningStack<int> st;
        st.push(5);
        st.push(10);

        int x;
        check(st.pop(x) && x == 10, "FC Stack LIFO test 1");
        check(st.pop(x) && x == 5,  "FC Stack LIFO test 2");
        check(!st.pop(x), "FC Stack empty pop");
    }

    // ---- Test FlatCombiningQueue ----
    {
        FlatCombiningQueue<int> q;
        q.enqueue(1);
        q.enqueue(2);

        int x;
        check(q.dequeue(x) && x == 1, "FC Queue FIFO test 1");
        check(q.dequeue(x) && x == 2, "FC Queue FIFO test 2");
        check(!q.dequeue(x), "FC Queue empty dequeue");
    }

    std::cout << "test_flat_combining OK\n";
    return 0;
}
