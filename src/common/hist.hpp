// Log-linear latency histogram (HdrHistogram-style, ~1.6% relative error).
// Values < 128 ns are exact; above that, each power of two is split into 64
// buckets. Fixed memory (~30 KB), O(1) record, mergeable.
#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace tms {

class Hist {
public:
    static constexpr int kSub = 64;
    static constexpr size_t kBuckets = 128 + 58 * kSub;

    void record(uint64_t v) {
        ++b_[index(v)];
        ++n_;
        max_ = std::max(max_, v);
    }
    void merge(const Hist& o) {
        for (size_t i = 0; i < kBuckets; ++i) b_[i] += o.b_[i];
        n_ += o.n_;
        max_ = std::max(max_, o.max_);
    }
    uint64_t count() const { return n_; }
    uint64_t max() const { return max_; }

    // Upper edge of the bucket holding the p-th percentile (never under-reports).
    uint64_t percentile(double p) const {
        if (n_ == 0) return 0;
        uint64_t target = static_cast<uint64_t>(p / 100.0 * n_ + 0.999999);
        if (target == 0) target = 1;
        uint64_t seen = 0;
        for (size_t i = 0; i < kBuckets; ++i) {
            seen += b_[i];
            if (seen >= target) return std::min(upper(i), max_);
        }
        return max_;
    }

    static size_t index(uint64_t v) {
        if (v < 128) return v;
        const int msb = 63 - __builtin_clzll(v);  // >= 7
        const int shift = msb - 6;                // mantissa in [64,128)
        const uint64_t mant = v >> shift;
        return 128 + static_cast<size_t>(shift - 1) * kSub + (mant - 64);
    }
    static uint64_t upper(size_t i) {
        if (i < 128) return i;
        const size_t k = i - 128;
        const int shift = static_cast<int>(k / kSub) + 1;
        const uint64_t mant = 64 + k % kSub;
        return ((mant + 1) << shift) - 1;
    }

private:
    std::array<uint64_t, kBuckets> b_{};
    uint64_t n_ = 0, max_ = 0;
};

}  // namespace tms
