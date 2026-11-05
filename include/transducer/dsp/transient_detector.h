#pragma once

#include <cmath>
#include <algorithm>
#include <array>

namespace transducer::dsp {

struct TransientResult {
    bool onset_detected = false;
    float velocity = 0.0f;     // 0.0 to 1.0
    float envelope = 0.0f;     // current RMS/energy follower
};

class TransientDetector {
public:
    static constexpr size_t kDelayLen = 128; // ~2.67ms @ 48kHz

    explicit TransientDetector(float threshold = 0.02f)
        : threshold_(threshold) {
        delay_ring_.fill(0.0f);
    }

    void reset() {
        env_ = 0.0f;
        delay_ring_.fill(0.0f);
        ring_pos_ = 0;
        lockout_samples_ = 0;
    }

    void set_threshold(float threshold) noexcept {
        threshold_ = threshold;
    }

    TransientResult process(float sample) {
        TransientResult res{};
        float abs_x = std::fabs(sample);

        // Fast peak follower
        if (abs_x > env_) {
            env_ = 0.80f * env_ + 0.20f * abs_x;
        } else {
            env_ = 0.999f * env_ + 0.001f * abs_x;
        }

        res.envelope = env_;

        // Delayed envelope from 128 samples ago
        float delayed_env = delay_ring_[ring_pos_];
        delay_ring_[ring_pos_] = env_;
        ring_pos_ = (ring_pos_ + 1) & (kDelayLen - 1);

        if (lockout_samples_ > 0) {
            --lockout_samples_;
            return res;
        }

        // Onset detected when energy jumps significantly compared to ~2.6ms ago
        float diff = env_ - delayed_env;
        if (env_ > threshold_ && diff > threshold_) {
            res.onset_detected = true;
            // Map attack slope to velocity (0.1 to 1.0)
            res.velocity = std::clamp(diff * 5.0f, 0.1f, 1.0f);
            lockout_samples_ = 384; // ~8ms lockout to avoid double triggering on string bounce
        }

        return res;
    }

    float current_envelope() const { return env_; }

private:
    float threshold_{0.02f};
    float env_{0.0f};
    std::array<float, kDelayLen> delay_ring_{};
    size_t ring_pos_{0};
    int lockout_samples_{0};
};

} // namespace transducer::dsp
