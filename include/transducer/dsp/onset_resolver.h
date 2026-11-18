#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace transducer::dsp {

struct ResolvedNote {
    int16_t key{0};
    float midi{0.0f};
    float velocity{0.0f};
};

// Bridges the 1-sample onset flag and the once-per-hop pitch estimate.
// Onset -> pending. A note is released only when (a) the peak-held velocity is known and
// (b) `kStableFrames` consecutive confident pitch frames agree, computed on post-onset audio only
// (YinPitchTracker::notify_onset limits the analysis window). Times out without emitting, so
// fret noise / unpitched hits do not produce notes.
class OnsetResolver {
public:
    static constexpr int kStableFrames = 2;
    static constexpr float kMinConfidence = 0.7f;
    static constexpr float kStableToleranceSemitones = 0.6f;
    static constexpr float kTimeoutSeconds = 0.150f;

    void prepare(float sample_rate) noexcept {
        timeout_samples_ = static_cast<int>(kTimeoutSeconds * sample_rate);
        reset();
    }

    void reset() noexcept {
        pending_ = false;
        vel_ready_ = false;
        stable_ = 0;
        age_ = 0;
    }

    bool pending() const noexcept { return pending_; }

    void on_onset() noexcept {
        pending_ = true;
        vel_ready_ = false;
        stable_ = 0;
        cand_ = 0.0f;
        age_ = 0;
    }

    void on_velocity(float v) noexcept {
        if (!pending_) return;
        vel_ready_ = true;
        velocity_ = v;
    }

    // Call once per sample. Returns true if the pending onset just timed out.
    bool tick() noexcept {
        if (!pending_) return false;
        if (++age_ > timeout_samples_) {
            pending_ = false;
            return true;
        }
        return false;
    }

    // Call once per pitch hop with the continuity-filtered pitch. Returns true and fills `out`
    // when the note should be emitted now.
    bool on_pitch(bool valid, float midi, float confidence, ResolvedNote& out) noexcept {
        if (!pending_) return false;
        if (!valid || confidence < kMinConfidence) {
            stable_ = 0;
            return false;
        }
        if (stable_ > 0 && std::fabs(midi - cand_) < kStableToleranceSemitones) {
            ++stable_;
        } else {
            stable_ = 1;
        }
        cand_ = midi;
        if (stable_ >= kStableFrames && vel_ready_) {
            out.midi = midi;
            out.key = static_cast<int16_t>(std::clamp(std::round(midi), 0.0f, 127.0f));
            out.velocity = velocity_;
            pending_ = false;
            return true;
        }
        return false;
    }

private:
    bool pending_{false};
    bool vel_ready_{false};
    float velocity_{0.0f};
    float cand_{0.0f};
    int stable_{0};
    int age_{0};
    int timeout_samples_{7200};
};

} // namespace transducer::dsp
