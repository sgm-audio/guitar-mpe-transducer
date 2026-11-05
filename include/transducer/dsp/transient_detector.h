#pragma once

#include <cmath>
#include <algorithm>

namespace transducer::dsp {

struct TransientResult {
    bool onset_detected = false;
    float velocity = 0.0f;     // 0.0 to 1.0
    float envelope = 0.0f;     // current RMS/energy follower
};

class TransientDetector {
public:
    explicit TransientDetector(float threshold = 0.015f)
        : threshold_(threshold) {}

    void reset() {
        env_fast_ = 0.0f;
        env_slow_ = 0.0f;
        lockout_samples_ = 0;
    }

    TransientResult process(float sample) {
        TransientResult res{};
        float abs_x = std::fabs(sample);

        // Fast envelope tracking attack (~1ms at 48kHz)
        env_fast_ = 0.92f * env_fast_ + 0.08f * abs_x;
        // Slow envelope tracking ambient decay (~50ms)
        env_slow_ = 0.998f * env_slow_ + 0.002f * abs_x;

        res.envelope = env_fast_;

        if (lockout_samples_ > 0) {
            --lockout_samples_;
            return res;
        }

        // Onset occurs when fast envelope exceeds slow envelope by threshold
        float diff = env_fast_ - env_slow_;
        if (env_fast_ > threshold_ && diff > 0.010f) {
            res.onset_detected = true;
            // Map attack slope to velocity (0.1 to 1.0)
            res.velocity = std::clamp(diff * 15.0f, 0.1f, 1.0f);
            lockout_samples_ = 256; // ~5.3ms lockout to avoid double triggering on string bounce
        }

        return res;
    }

    float current_envelope() const { return env_fast_; }

private:
    float threshold_{0.015f};
    float env_fast_{0.0f};
    float env_slow_{0.0f};
    int lockout_samples_{0};
};

} // namespace transducer::dsp
