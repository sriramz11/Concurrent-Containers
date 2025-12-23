#pragma once

#include "common.h"
#include <deque>
#include <list>
#include <mutex>
#include <atomic>

template <typename T>
class FlatCombiningQueue {
public:
    FlatCombiningQueue() = default;

    bool empty()
    {
        std::lock_guard<std::mutex> lk(combine_m_);
        return data_.empty();
    }

    void enqueue(const T& v)
    {
        Request& r = get_request();
        r.value    = v;
        r.success  = true;  // enqueue always succeeds
        r.op.store(Request::Op::ENQ, std::memory_order_release);

        combine();
        // after combine(), enqueue is done
    }

    bool dequeue(T& out)
    {
        Request& r = get_request();
        r.success  = false;
        r.op.store(Request::Op::DEQ, std::memory_order_release);

        combine();
        // after combine(), success/value are stable
        if (r.success)
            out = r.value;
        return r.success;
    }

private:
    struct Request {
        enum class Op { NONE, ENQ, DEQ };
        std::atomic<Op> op{Op::NONE};
        T               value{};
        bool            success{false};
    };

    std::mutex          combine_m_;
    std::deque<T>       data_;
    std::list<Request*> requests_;
    thread_local static Request* tls_req_;

    Request& get_request()
    {
        if (!tls_req_) {
            auto* r = new Request();  // intentionally never freed
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
            if (op == Request::Op::ENQ) {
                data_.push_back(r->value);
                // success already true
                r->op.store(Request::Op::NONE, std::memory_order_release);
            } else if (op == Request::Op::DEQ) {
                if (!data_.empty()) {
                    r->value   = data_.front();
                    data_.pop_front();
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
thread_local typename FlatCombiningQueue<T>::Request*
    FlatCombiningQueue<T>::tls_req_ = nullptr;
