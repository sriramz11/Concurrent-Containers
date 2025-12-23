#pragma once
#include <optional>

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

// ==== include ALL data structures here ====

#include "sgl_stack.h"
#include "treiber_stack.h"
#include "elimination_stack.h"
#include "flat_combining_stack.h"

#include "sgl_queue.h"
#include "ms_queue.h"
#include "flat_combining_queue.h"
// Simple timing helper
inline std::uint64_t now_ns()
{
    using clock = std::chrono::steady_clock;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               clock::now().time_since_epoch())
        .count();
}

// Simple assert helper
inline void check(bool cond, const char* msg)
{
    if (!cond) {
        std::cerr << "CHECK FAILED: " << msg << std::endl;
        std::abort();
    }
}

// Random generator per thread
inline std::mt19937& thread_rng()
{
    thread_local std::mt19937 gen{std::random_device{}()};
    return gen;
}
