#pragma once

#include <clap/clap.h>
#include <atomic>
#include <thread>

namespace transducer::common {

class ThreadCheck {
public:
    ThreadCheck() : host_(nullptr), ext_(nullptr) {
        main_thread_id_ = std::this_thread::get_id();
        audio_thread_id_.store(std::thread::id{}, std::memory_order_relaxed);
        has_audio_id_.store(false, std::memory_order_relaxed);
    }

    void set_host(const clap_host_t* host) {
        host_ = host;
        if (host && host->get_extension) {
            ext_ = static_cast<const clap_host_thread_check_t*>(
                host->get_extension(host, CLAP_EXT_THREAD_CHECK));
        } else {
            ext_ = nullptr;
        }
    }

    bool is_main_thread() const {
        if (ext_ && ext_->is_main_thread) {
            return ext_->is_main_thread(host_);
        }
        return std::this_thread::get_id() == main_thread_id_;
    }

    bool is_audio_thread() const {
        if (ext_ && ext_->is_audio_thread) {
            return ext_->is_audio_thread(host_);
        }
        if (has_audio_id_.load(std::memory_order_acquire)) {
            return std::this_thread::get_id() ==
                audio_thread_id_.load(std::memory_order_acquire);
        }
        return false;
    }

    void mark_audio_thread() {
        bool expected = false;
        if (has_audio_id_.compare_exchange_strong(
                expected, true, std::memory_order_acq_rel)) {
            audio_thread_id_.store(std::this_thread::get_id(),
                std::memory_order_release);
        }
    }

    const clap_host_thread_check_t* host_ext() const { return ext_; }

private:
    const clap_host_t* host_;
    const clap_host_thread_check_t* ext_;
    std::thread::id main_thread_id_;
    std::atomic<std::thread::id> audio_thread_id_;
    std::atomic<bool> has_audio_id_;
};

} // namespace transducer::common
