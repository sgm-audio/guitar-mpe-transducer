#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace transducer::dsp {

// Release velocity from the *rate of level decay in dB/ms*.
//
// Use `update(level)` once per pitch hop (prepare() with the hop rate). A natural ringout falls
// ~0.01-0.05 dB/ms, a palm mute ~1 dB/ms, a hard choke 10+ dB/ms. The estimate peak-holds the
// steepest recent slope (20 ms decay), so it still reads high when note-off fires a few hops
// after the choke. `muted()` reports a >= 30 dB drop over the last ~10 ms, which a naturally
// decaying string cannot produce.
//
// The legacy `process(envelope)` (linear drop per call) is kept for the block-based engine.
class ReleaseVelocityEstimator {
public:
    static constexpr float kMuteDropDb = 30.0f;
    static constexpr float kMuteWindowMs = 10.0f;
    static constexpr float kFullScaleSlopeDbPerMs = 4.0f;

    ReleaseVelocityEstimator() { reset(); }

    // `update_rate_hz`: how often update() is called (sample_rate / hop).
    void prepare(float update_rate_hz) noexcept {
        dt_ms_ = 1000.0f / std::max(update_rate_hz, 1.0f);
        hold_decay_ = std::exp(-dt_ms_ / 20.0f);
        window_len_ = std::clamp(static_cast<size_t>(std::ceil(kMuteWindowMs / dt_ms_)) + 1,
                                 static_cast<size_t>(2), kMaxHistory);
        reset();
    }

    void reset() noexcept {
        prev_envelope_ = 0.0f;
        release_velocity_ = 0.1f;
        history_.fill(kFloorDb);
        write_ = 0;
        filled_ = 0;
        peak_slope_ = 0.0f;
        prev_db_ = kFloorDb;
        muted_ = false;
        drop_db_ = 0.0f;
    }

    // Per-hop update from a fast level follower. Returns release velocity [0.05, 1.0].
    float update(float level) noexcept {
        const float db = to_db(level);
        const float slope = (prev_db_ - db) / dt_ms_; // dB/ms, positive while falling
        prev_db_ = db;
        peak_slope_ = std::max(slope, peak_slope_ * hold_decay_);
        release_velocity_ = std::clamp(peak_slope_ / kFullScaleSlopeDbPerMs, 0.05f, 1.0f);

        history_[write_] = db;
        write_ = (write_ + 1) % window_len_;
        if (filled_ < window_len_) ++filled_;
        // `write_` now indexes the oldest entry in the window.
        const float oldest = (filled_ == window_len_) ? history_[write_] : history_[0];
        drop_db_ = oldest - db;
        muted_ = (filled_ == window_len_) && drop_db_ >= kMuteDropDb && oldest > kAudibleDb;
        return release_velocity_;
    }

    bool muted() const noexcept { return muted_; }
    float drop_db() const noexcept { return drop_db_; }

    // Legacy per-call linear-envelope estimator (block-based engine, existing tests).
    float process(float current_envelope) {
        float drop = prev_envelope_ - current_envelope;
        prev_envelope_ = current_envelope;

        if (drop > 0.0f) {
            float instant_vel = std::clamp(drop * 2.5f, 0.05f, 1.0f);
            if (instant_vel > release_velocity_) {
                release_velocity_ = instant_vel;
            } else {
                release_velocity_ = 0.8f * release_velocity_ + 0.2f * instant_vel;
            }
        } else {
            release_velocity_ = 0.95f * release_velocity_ + 0.05f * 0.1f;
        }

        return release_velocity_;
    }

    float release_velocity() const { return release_velocity_; }

private:
    static constexpr float kFloorDb = -120.0f;
    static constexpr float kAudibleDb = -70.0f; // ignore "mutes" of inaudible residue
    static constexpr size_t kMaxHistory = 64;

    static float to_db(float level) noexcept {
        return 20.0f * std::log10(std::max(level, 1e-6f));
    }

    float prev_envelope_{0.0f};
    float release_velocity_{0.1f};

    float dt_ms_{1000.0f / 750.0f};
    float hold_decay_{0.94f};
    size_t window_len_{9};
    std::array<float, kMaxHistory> history_{};
    size_t write_{0};
    size_t filled_{0};
    float peak_slope_{0.0f};
    float prev_db_{kFloorDb};
    bool muted_{false};
    float drop_db_{0.0f};
};

} // namespace transducer::dsp
