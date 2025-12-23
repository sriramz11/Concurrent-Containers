#include "elimination_stack.h"
#include "common.h"
#include <iostream>

int main() {
    EliminationStack<int> st;

    // basic push/pop test
    st.push(10);
    st.push(20);

    int x;
    check(st.pop(x) && x == 20, "EliminationStack LIFO test 1");
    check(st.pop(x) && x == 10, "EliminationStack LIFO test 2");
    check(!st.pop(x), "EliminationStack empty pop");

    std::cout << "test_elimination OK\n";
    return 0;
}
