#pragma once

#include "common.h"
#include <vector>

template <typename T>
class SGLStack {
public:
    SGLStack() = default;

    void push(const T& value)
    {
        std::lock_guard<std::mutex> lk(m_);
        data_.push_back(value);
    }

    bool pop(T& out)
    {
        std::lock_guard<std::mutex> lk(m_);
        if (data_.empty())
            return false;
        out = data_.back();
        data_.pop_back();
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
    std::vector<T>     data_;
};
