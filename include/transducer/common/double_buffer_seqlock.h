#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace transducer::common {

// Single-writer / multi-reader seqlock for small trivially-copyable snapshots.
//
//  * Sequence counter is odd while a write is in progress, so a reader that overlaps a write
//    always detects it (read() returns false) -- no torn snapshot is ever returned as valid.
//  * The payload is stored in std::atomic<uint64_t> words (relaxed), so there is no data race.
//  * Writers are wait-free; readers are lock-free (callers retry or keep their last good copy).
//  * Exactly ONE thread may write at a time. For multiple writers use one atomic per value.
//
// begin_write()/end_write() edit a writer-private staging copy that already contains the last
// published value (read-modify-write needs no reader access). `swap()` is kept as a no-op for
// source compatibility with the former double-buffer implementation.
template<typename T>
class DoubleBufferSeqLock {
    static_assert(std::is_trivially_copyable_v<T>,
        "DoubleBufferSeqLock<T> requires T to be trivially copyable");
    static constexpr size_t kWords = (sizeof(T) + sizeof(uint64_t) - 1) / sizeof(uint64_t);

public:
    DoubleBufferSeqLock() {
        staging_ = T{};
        publish(staging_);
        seq_.store(0, std::memory_order_relaxed);
    }

    T* begin_write() { return &staging_; }
    void end_write() { publish(staging_); }

    void write(const T& value) {
        staging_ = value;
        publish(staging_);
    }

    // Returns false if a write overlapped the read; `out` is then unspecified.
    bool read(T& out) const {
        const uint64_t s1 = seq_.load(std::memory_order_acquire);
        if (s1 & 1u) return false;
        uint64_t tmp[kWords];
        for (size_t i = 0; i < kWords; ++i) {
            tmp[i] = words_[i].load(std::memory_order_relaxed);
        }
        std::atomic_thread_fence(std::memory_order_acquire);
        if (seq_.load(std::memory_order_relaxed) != s1) return false;
        std::memcpy(&out, tmp, sizeof(T));
        return true;
    }

    void swap() {}

    uint64_t generation() const { return seq_.load(std::memory_order_acquire) / 2; }

private:
    void publish(const T& v) {
        uint64_t tmp[kWords] = {};
        std::memcpy(tmp, &v, sizeof(T));
        const uint64_t s = seq_.load(std::memory_order_relaxed);
        seq_.store(s + 1, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (size_t i = 0; i < kWords; ++i) {
            words_[i].store(tmp[i], std::memory_order_relaxed);
        }
        seq_.store(s + 2, std::memory_order_release);
    }

    alignas(64) std::atomic<uint64_t> seq_{0};
    std::atomic<uint64_t> words_[kWords]{};
    T staging_{};
};

} // namespace transducer::common
