// TelemetryBuffer.hpp  --  UNIT II : dynamic memory, parameterized / dynamic
//                          constructors, deep copy, explicit destructor
#pragma once
#include <algorithm>
#include <cstddef>
#include <ostream>
#include <stdexcept>

namespace DisasterCore::Sensors {

// Fixed-capacity ring buffer of samples living on the heap.
class TelemetryBuffer {
    double*     data_;
    std::size_t capacity_;
    std::size_t count_;   // valid samples
    std::size_t head_;    // next slot to write

public:
    // Parameterized + dynamic constructor (allocates at run time)
    explicit TelemetryBuffer(std::size_t capacity = 8)
        : data_(new double[capacity ? capacity : 1]()),
          capacity_(capacity ? capacity : 1), count_(0), head_(0) {}

    // Dynamic constructor from a raw array
    TelemetryBuffer(const double* src, std::size_t n)
        : data_(new double[n ? n : 1]()), capacity_(n ? n : 1), count_(n), head_(0) {
        std::copy(src, src + n, data_);
    }

    // Deep copy constructor : the copy owns its own heap block
    TelemetryBuffer(const TelemetryBuffer& o)
        : data_(new double[o.capacity_]), capacity_(o.capacity_),
          count_(o.count_), head_(o.head_) {
        std::copy(o.data_, o.data_ + o.capacity_, data_);
    }

    // Deep copy assignment (copy-then-swap style, strong exception safety)
    TelemetryBuffer& operator=(const TelemetryBuffer& o) {
        if (this != &o) {
            double* fresh = new double[o.capacity_];
            std::copy(o.data_, o.data_ + o.capacity_, fresh);
            delete[] data_;
            data_ = fresh;
            capacity_ = o.capacity_;
            count_ = o.count_;
            head_ = o.head_;
        }
        return *this;
    }

    // Explicit destructor : no leaks
    ~TelemetryBuffer() { delete[] data_; }

    void push(double v) {
        data_[head_] = v;
        head_ = (head_ + 1) % capacity_;
        if (count_ < capacity_) ++count_;
    }

    std::size_t size() const     { return count_; }
    std::size_t capacity() const { return capacity_; }

    // Return by reference : i-th oldest sample (modifiable)
    double& at(std::size_t i) {
        if (i >= count_) throw std::out_of_range("TelemetryBuffer::at");
        return data_[(head_ + capacity_ - count_ + i) % capacity_];
    }
    const double& at(std::size_t i) const {     // const-correct overload
        if (i >= count_) throw std::out_of_range("TelemetryBuffer::at");
        return data_[(head_ + capacity_ - count_ + i) % capacity_];
    }

    double mean() const {
        if (count_ == 0) return 0.0;
        double s = 0.0;
        for (std::size_t i = 0; i < count_; ++i) s += at(i);
        return s / static_cast<double>(count_);
    }

    friend std::ostream& operator<<(std::ostream& os, const TelemetryBuffer& b) {
        os << '[';
        for (std::size_t i = 0; i < b.count_; ++i) os << (i ? ", " : "") << b.at(i);
        return os << ']';
    }
};

} // namespace DisasterCore::Sensors
