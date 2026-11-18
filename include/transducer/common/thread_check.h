#pragma once

#include <clap/clap.h>
#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

namespace transducer::common {

class ThreadCheck {
    static_assert(std::atomic<uint64_t>::is_always_lock_free,
                  "Audio thread ID check must be lock-free");

public:
    ThreadCheck() : host_(nullptr), ext_(nullptr) {
        main_thread_id_ = hash_id(std::this_thread::get_id());
    }

    void set_host(const clap_host_t* host) noexcept {
        host_ = host;
        if (host && host->get_extension) {
            ext_ = static_cast<const clap_host_thread_check_t*>(
                host->get_extension(host, CLAP_EXT_THREAD_CHECK));
        } else {
            ext_ = nullptr;
        }
    }

    void reset_audio_thread() noexcept {
        has_audio_id_.store(false, std::memory_order_release);
    }

    bool is_main_thread() const noexcept {
        if (ext_ && ext_->is_main_thread) {
            return ext_->is_main_thread(host_);
        }
        return hash_id(std::this_thread::get_id()) == main_thread_id_;
    }

    bool is_audio_thread() const noexcept {
        if (ext_ && ext_->is_audio_thread) {
            return ext_->is_audio_thread(host_);
        }
        if (has_audio_id_.load(std::memory_order_acquire)) {
            return hash_id(std::this_thread::get_id()) ==
                   audio_thread_id_.load(std::memory_order_acquire);
        }
        return false;
    }

    void mark_audio_thread() noexcept {
        if (!has_audio_id_.load(std::memory_order_acquire)) {
            audio_thread_id_.store(hash_id(std::this_thread::get_id()), std::memory_order_release);
            has_audio_id_.store(true, std::memory_order_release);
        }
    }

    const clap_host_thread_check_t* host_ext() const noexcept { return ext_; }

private:
    static uint64_t hash_id(std::thread::id id) noexcept {
        return static_cast<uint64_t>(std::hash<std::thread::id>{}(id));
    }

    const clap_host_t* host_;
    const clap_host_thread_check_t* ext_;
    uint64_t main_thread_id_{0};
    std::atomic<uint64_t> audio_thread_id_{0};
    std::atomic<bool> has_audio_id_{false};
};

} // namespace transducer::common
