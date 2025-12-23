#pragma once

#include "common.h"
#include <vector>
#include <list>
#include <mutex>
#include <atomic>

template <typename T>
class FlatCombiningStack {
public:
    FlatCombiningStack() = default;

    void push(const T& v)
    {
        Request& r = get_request();
        r.value    = v;
        r.success  = true;  // push always succeeds
        r.op.store(Request::Op::PUSH, std::memory_order_release);

        combine();
        // after combine(), our request has been processed
    }

    bool pop(T& out)
    {
        Request& r = get_request();
        r.success  = false;
        r.op.store(Request::Op::POP, std::memory_order_release);

        combine();
        // after combine(), success/value are stable
        if (r.success)
            out = r.value;
        return r.success;
    }

    bool empty()
    {
        std::lock_guard<std::mutex> lk(combine_m_);
        return data_.empty();
    }

private:
    struct Request {
        enum class Op { NONE, PUSH, POP };
        std::atomic<Op> op{Op::NONE};
        T               value{};
        bool            success{false};
    };

    std::mutex          combine_m_;
    std::vector<T>      data_;
    std::list<Request*> requests_;

    // Pointer, not object: lifetime not tied to thread exit
    thread_local static Request* tls_req_;

    Request& get_request()
    {
        // One Request per thread, allocated once and registered under lock
        if (!tls_req_) {
            auto* r = new Request();  // intentionally never freed (tiny bounded leak)
            {
                std::lock_guard<std::mutex> lk(combine_m_);
                requests_.push_back(r);
            }
            tls_req_ = r;
        }
        return *tls_req_;
    }

    void combine()
    {
        std::lock_guard<std::mutex> lk(combine_m_);
        for (Request* r : requests_) {
            auto op = r->op.load(std::memory_order_acquire);
            if (op == Request::Op::PUSH) {
                data_.push_back(r->value);
                // success already true
                r->op.store(Request::Op::NONE, std::memory_order_release);
            } else if (op == Request::Op::POP) {
                if (!data_.empty()) {
                    r->value   = data_.back();
                    data_.pop_back();
                    r->success = true;
                } else {
                    r->success = false;
                }
                r->op.store(Request::Op::NONE, std::memory_order_release);
            }
        }
    }
};

template <typename T>
thread_local typename FlatCombiningStack<T>::Request*
    FlatCombiningStack<T>::tls_req_ = nullptr;
