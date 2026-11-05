#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace transducer::common {

class DoubleBufferSeqLockCore {
public:
    static constexpr uint32_t kSlots = 2;

    void init(void* slot_storage, size_t slot_size) {
        slot_size_ = slot_size;
        slots_ = static_cast<std::byte*>(slot_storage);
        for (size_t i = 0; i < slot_size * kSlots; ++i) {
            slots_[i] = std::byte{0};
        }
        front_.store(0, std::memory_order_relaxed);
        writer_.store(0, std::memory_order_relaxed);
        generation_.store(0, std::memory_order_relaxed);
    }

    void* begin_write() {
        return &slots_[writer_.load(std::memory_order_acquire) * slot_size_];
    }

    void end_write() {
        uint32_t cur = writer_.load(std::memory_order_relaxed);
        front_.store(cur, std::memory_order_release);
        generation_.fetch_add(1, std::memory_order_acq_rel);
    }

    bool read(void* out) const {
        uint64_t g1 = generation_.load(std::memory_order_acquire);
        uint32_t f = front_.load(std::memory_order_acquire);
        std::memcpy(out, &slots_[f * slot_size_], slot_size_);
        uint64_t g2 = generation_.load(std::memory_order_acquire);
        return g1 == g2;
    }

    void swap() {
        uint32_t cur = writer_.load(std::memory_order_acquire);
        uint32_t next = cur ^ 1u;
        writer_.store(next, std::memory_order_release);
    }

    uint64_t generation() const {
        return generation_.load(std::memory_order_acquire);
    }

    size_t slot_size() const { return slot_size_; }

private:
    std::byte* slots_{nullptr};
    size_t slot_size_{0};
    alignas(64) std::atomic<uint32_t> front_{0};
    alignas(64) std::atomic<uint32_t> writer_{0};
    alignas(64) std::atomic<uint64_t> generation_{0};
};

template<typename T>
class DoubleBufferSeqLock {
    static_assert(std::is_trivially_copyable_v<T>,
        "DoubleBufferSeqLock<T> requires T to be trivially copyable");

public:
    DoubleBufferSeqLock() {
        core_.init(slots_, sizeof(T));
    }

    T* begin_write() {
        return reinterpret_cast<T*>(core_.begin_write());
    }

    void end_write() {
        core_.end_write();
    }

    bool read(T& out) const {
        return core_.read(&out);
    }

    void swap() { core_.swap(); }

    uint64_t generation() const { return core_.generation(); }

private:
    alignas(64) std::byte slots_[sizeof(T) * DoubleBufferSeqLockCore::kSlots];
    DoubleBufferSeqLockCore core_;
};

} // namespace transducer::common
