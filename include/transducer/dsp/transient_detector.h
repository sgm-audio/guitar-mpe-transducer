#pragma once

#include <cmath>
#include <algorithm>
#include <cstddef>

namespace transducer::dsp {

struct TransientResult {
    bool onset_detected = false;
    // Provisional velocity at the trigger instant (always low: the attack has barely started).
    // Kept for legacy block-based callers. Use `velocity_ready` / `final_velocity` for note-on.
    float velocity = 0.0f;
    float envelope = 0.0f;     // legacy peak follower (slow release), used for gate / pressure

    // Set exactly once per onset, kPeakHoldSeconds after the trigger.
    bool velocity_ready = false;
    float final_velocity = 0.0f; // 0.05 .. 1.0 from peak-held attack level (dB mapped)
};

// Onset detection from two envelope followers:
//   fast (0.3 ms attack / 10 ms release) vs slow (15 ms attack / 100 ms release).
// Onset when fast > slow * ratio and fast > threshold (absolute floor).
// The ratio test is level-relative, so it still fires on a pluck over a decaying string and does
// not fire on slow swells. Velocity is measured from the attack peak (peak-hold), not at trigger.
class TransientDetector {
public:
    static constexpr float kPeakHoldSeconds = 0.005f;
    static constexpr float kLockoutSeconds = 0.008f;

    explicit TransientDetector(float threshold = 0.02f)
        : threshold_(threshold) {
        set_sample_rate(48000.0f);
    }

    void set_sample_rate(float sample_rate) noexcept {
        sample_rate_ = std::max(sample_rate, 1000.0f);
        fast_att_ = coeff(0.0003f);
        fast_rel_ = coeff(0.010f);
        slow_att_ = coeff(0.015f);
        slow_rel_ = coeff(0.100f);
        hold_len_ = static_cast<int>(kPeakHoldSeconds * sample_rate_);
        lockout_len_ = static_cast<int>(kLockoutSeconds * sample_rate_);
    }

    void reset() noexcept {
        env_ = 0.0f;
        fast_ = 0.0f;
        slow_ = 0.0f;
        lockout_samples_ = 0;
        armed_ = true;
        hold_remaining_ = 0;
        hold_peak_ = 0.0f;
    }

    void set_threshold(float threshold) noexcept { threshold_ = threshold; }
    void set_ratio(float ratio) noexcept { ratio_ = std::max(ratio, 1.05f); }

    TransientResult process(float sample) noexcept {
        TransientResult res{};
        const float abs_x = std::fabs(sample);

        // Legacy envelope (gate, pressure, release detection). Unchanged behaviour.
        if (abs_x > env_) {
            env_ = 0.80f * env_ + 0.20f * abs_x;
        } else {
            env_ = 0.999f * env_ + 0.001f * abs_x;
        }
        if (env_ < kFlush) env_ = 0.0f; // avoid denormals in silence
        res.envelope = env_;

        fast_ += (abs_x > fast_ ? fast_att_ : fast_rel_) * (abs_x - fast_);
        slow_ += (abs_x > slow_ ? slow_att_ : slow_rel_) * (abs_x - slow_);
        if (fast_ < kFlush) fast_ = 0.0f;
        if (slow_ < kFlush) slow_ = 0.0f;

        // Peak-hold after a trigger -> final velocity.
        if (hold_remaining_ > 0) {
            hold_peak_ = std::max(hold_peak_, fast_);
            if (--hold_remaining_ == 0) {
                res.velocity_ready = true;
                res.final_velocity = map_velocity(hold_peak_);
            }
        }

        // Re-arm only after the fast envelope has fallen back toward the slow one; otherwise a
        // single attack (slow envelope still catching up) would fire repeatedly.
        if (!armed_ && fast_ < slow_ * kRearmRatio) armed_ = true;

        if (lockout_samples_ > 0) {
            --lockout_samples_;
            return res;
        }
        if (!armed_) return res;

        if (fast_ > threshold_ && fast_ > slow_ * ratio_) {
            res.onset_detected = true;
            armed_ = false;
            res.velocity = map_velocity(fast_);
            lockout_samples_ = lockout_len_;
            hold_remaining_ = std::max(hold_len_, 1);
            hold_peak_ = fast_;
        }

        return res;
    }

    float current_envelope() const { return env_; }

private:
    static constexpr float kFlush = 1e-20f;
    static constexpr float kRearmRatio = 1.25f;

    float coeff(float seconds) const noexcept {
        return 1.0f - std::exp(-1.0f / (seconds * sample_rate_));
    }

    // -40 dBFS -> 0.05 (floor), -4 dBFS -> 1.0
    static float map_velocity(float peak) noexcept {
        const float db = 20.0f * std::log10(std::max(peak, 1e-6f));
        return std::clamp((db + 40.0f) / 36.0f, 0.05f, 1.0f);
    }

    float threshold_{0.02f};
    float ratio_{1.8f};
    float sample_rate_{48000.0f};
    float fast_att_{0.0f}, fast_rel_{0.0f}, slow_att_{0.0f}, slow_rel_{0.0f};
    int hold_len_{240};
    int lockout_len_{384};

    float env_{0.0f};
    float fast_{0.0f};
    float slow_{0.0f};
    int lockout_samples_{0};
    bool armed_{true};
    int hold_remaining_{0};
    float hold_peak_{0.0f};
};

} // namespace transducer::dsp
