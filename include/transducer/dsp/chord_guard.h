#pragma once

#include <cstdint>

namespace transducer::dsp {

// A single mono pickup cannot be tracked polyphonically. Two onsets within `kWindowSeconds`
// (a strummed dyad/chord; no monophonic line is that fast) mark a chord. While the chord is held
// (kHoldSeconds after the most recent onset) every further onset is suppressed, so the caller
// emits no wrong monophonic notes. Use hexaphonic input for real polyphony.
class ChordGuard {
public:
    static constexpr float kWindowSeconds = 0.045f;
    static constexpr float kHoldSeconds = 0.250f;

    void prepare(float sample_rate) noexcept {
        window_ = static_cast<uint64_t>(kWindowSeconds * sample_rate);
        hold_ = static_cast<uint64_t>(kHoldSeconds * sample_rate);
        reset();
    }

    void reset() noexcept {
        have_last_ = false;
        last_ = 0;
        hold_until_ = 0;
    }

    // `t`: running sample clock. Returns true if this onset must be suppressed (chord).
    bool on_onset(uint64_t t) noexcept {
        const bool in_hold = t < hold_until_;
        const bool strum = have_last_ && (t - last_) < window_;
        last_ = t;
        have_last_ = true;
        if (in_hold || strum) {
            hold_until_ = t + hold_;
            return true;
        }
        return false;
    }

    bool holding(uint64_t t) const noexcept { return t < hold_until_; }

private:
    uint64_t window_{2160};
    uint64_t hold_{12000};
    uint64_t last_{0};
    uint64_t hold_until_{0};
    bool have_last_{false};
};

} // namespace transducer::dsp
