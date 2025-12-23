#pragma once

#include "common.h"
#include <array>
#include <algorithm>

namespace hp {

// Maximum number of threads that can simultaneously use hazard pointers
inline constexpr std::size_t MAX_THREADS = 64;
inline constexpr std::size_t HAZARD_SLOTS_PER_THREAD = 2;
inline constexpr std::size_t MAX_HAZARD_POINTERS =
    MAX_THREADS * HAZARD_SLOTS_PER_THREAD;

// A single hazard pointer record
struct HazardRecord {
    std::atomic<std::thread::id> owner;
    std::atomic<void*>           ptr;

    HazardRecord() : owner{}, ptr{nullptr} {}
};

inline HazardRecord g_hazard_records[MAX_HAZARD_POINTERS];

// Acquire a hazard pointer slot for this thread
inline HazardRecord* acquire_hazard_record()
{
    std::thread::id empty_id{};
    std::thread::id me = std::this_thread::get_id();

    // Try to reuse existing record
    for (auto& rec : g_hazard_records) {
        
        if (rec.owner.load(std::memory_order_relaxed) == me)
            return &rec;
    }

    // Grab a free one
    for (auto& rec : g_hazard_records) {
        std::thread::id expected{};
        if (rec.owner.compare_exchange_strong(
                expected, me, std::memory_order_acq_rel)) {
            return &rec;
        }
    }

    std::cerr << "No free hazard pointer records available\n";
    std::abort();
}

// RAII owner for one hazard pointer slot
class HazardPointerOwner {
public:
    HazardPointerOwner()
    {
        rec_ = acquire_hazard_record();
    }

    HazardPointerOwner(const HazardPointerOwner&) = delete;
    HazardPointerOwner& operator=(const HazardPointerOwner&) = delete;

    ~HazardPointerOwner()
    {
        clear();
    }

    void set(void* p)
    {
        rec_->ptr.store(p, std::memory_order_release);
    }

    void clear()
    {
        rec_->ptr.store(nullptr, std::memory_order_release);
    }

private:
    HazardRecord* rec_{nullptr};
};

// Collect all hazard pointer values currently in use
inline void collect_hazard_pointers(std::vector<void*>& out)
{
    out.clear();
    out.reserve(MAX_HAZARD_POINTERS);
    for (auto& rec : g_hazard_records) {
        void* p = rec.ptr.load(std::memory_order_acquire);
        if (p)
            out.push_back(p);
    }
}

// Per-thread retired list and reclamation logic
template <typename T>
class RetiredList {
public:
    using Deleter = std::function<void(T*)>;

    explicit RetiredList(Deleter d = [](T* p) { delete p; })
        : deleter_(std::move(d))
    {}

    void retire(T* node)
    {
        retired_.push_back(node);
        if (retired_.size() >= threshold_)
            scan();
    }

    // Force reclaim (e.g., in destructor of data structure)
    void force_reclaim()
    {
        scan(true);
    }

private:
    void scan(bool force_all = false)
    {
        std::vector<void*> hp_values;
        collect_hazard_pointers(hp_values);

        auto is_hazard = [&hp_values](void* p) {
            return std::find(hp_values.begin(), hp_values.end(), p) !=
                   hp_values.end();
        };

        std::vector<T*> kept;
        kept.reserve(retired_.size());

        for (T* p : retired_) {
            if (!force_all && is_hazard(p)) {
                kept.push_back(p);
            } else {
                deleter_(p);
            }
        }

        retired_.swap(kept);
    }

    std::vector<T*> retired_;
    Deleter         deleter_;
    std::size_t     threshold_ = 64; // tune if needed
};

// Get per-thread retired list
template <typename T>
inline RetiredList<T>& retired_list()
{
    thread_local RetiredList<T> rl{};
    return rl;
}

} // namespace hp
