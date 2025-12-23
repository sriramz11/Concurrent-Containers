#pragma once

#include "common.h"
#include <deque>

template <typename T>
class SGLQueue {
public:
    SGLQueue() = default;

    void enqueue(const T& value)
    {
        std::lock_guard<std::mutex> lk(m_);
        data_.push_back(value);
    }

    bool dequeue(T& out)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty())
            return false;
        out = data_.front();
        data_.pop_front();
        return true;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return data_.empty();
    }

    std::size_t size() const
    {
        std::lock_guard<std::mutex> lk(m_);
        return data_.size();
    }

private:
    mutable std::mutex m_;
    std::deque<T>      data_;
};
