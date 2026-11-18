#pragma once

#include <array>
#include <cmath>
#include <optional>
#include <algorithm>

namespace transducer::dsp {

struct PitchEstimate {
    float f0_hz{0.0f};
    float midi_note{0.0f}; // Continuous MIDI note (e.g. 69.34 for slightly sharp A4)
    float confidence{0.0f}; // 0.0 to 1.0
    bool is_valid{false};
};

class YinPitchTracker {
public:
    static constexpr int kWinSize = 4096;
    static constexpr int kMaxTau = 2048;
    static constexpr int kHopSize = 64; // Evaluate every 64 samples (~1.33ms @ 48kHz)

    YinPitchTracker(float threshold = 0.20f)
        : threshold_(threshold) {
        ring_.fill(0.0f);
        yin_buf_.fill(0.0f);
    }

    void reset() {
        ring_.fill(0.0f);
        yin_buf_.fill(0.0f);
        write_pos_ = 0;
        hop_counter_ = 0;
        valid_history_ = kWinSize;
        prev_tau_ = 0.0f;
        last_estimate_ = {};
    }

    // Call when an onset is detected, before feeding the onset sample. Until enough post-onset
    // samples exist, analysis is restricted to them (window W and lag range satisfy W + tau <=
    // samples since onset), so pre-onset audio (previous note) cannot produce a stale pitch.
    // Periods longer than half the post-onset history report invalid until history grows.
    void notify_onset() { valid_history_ = 0; prev_tau_ = 0.0f; }

    std::optional<PitchEstimate> processSample(float sample, float sample_rate) {
        ring_[write_pos_] = sample;
        write_pos_ = (write_pos_ + 1) & (kWinSize - 1);
        if (valid_history_ < kWinSize) ++valid_history_;

        if (++hop_counter_ < kHopSize) {
            return std::nullopt;
        }
        hop_counter_ = 0;

        PitchEstimate est = computePitch(sample_rate);
        last_estimate_ = est;
        return est;
    }

    const PitchEstimate& latest_estimate() const { return last_estimate_; }

private:
    PitchEstimate computePitch(float sample_rate) {
        PitchEstimate est{};
        
        // Check energy level first (avoid computing YIN on noise/silence)
        float energy = 0.0f;
        const int energy_len = std::min(kMaxTau, valid_history_);
        for (int i = 0; i < energy_len; ++i) {
            float s = ring_[(write_pos_ - 1 - i) & (kWinSize - 1)];
            energy += s * s;
        }
        if (energy_len == 0 || (energy / static_cast<float>(energy_len)) < 1e-6f) {
            return est;
        }

        // Linearise the ring (oldest -> newest) so the inner loops are unmasked and vectorisable.
        // Needed history: window W = max_tau plus lags up to max_tau.
        const int min_tau = std::max(10, static_cast<int>(sample_rate / 3000.0f));
        const int full_max_tau = std::min(kMaxTau - 2, static_cast<int>(sample_rate / 50.0f));
        const int max_tau = std::min({full_max_tau, valid_history_ / 2});
        if (max_tau <= min_tau + 2) {
            return est; // not enough post-onset history yet
        }
        const int win = max_tau;
        const int len = win + max_tau;
        {
            const int start = (write_pos_ - len) & (kWinSize - 1);
            const int first = std::min(len, kWinSize - start);
            std::copy_n(ring_.data() + start, first, lin_.data());
            if (first < len) std::copy_n(ring_.data(), len - first, lin_.data() + first);
        }
        const float* recent = lin_.data() + max_tau; // newest `win` samples

        // 1-2. Difference function + CMNDF, computed lag by lag. Stops at the first local minimum
        // below threshold (standard YIN early exit), so cost scales with the period, not max_tau.
        yin_buf_[0] = 1.0f;
        float running_sum = 0.0f;
        int best_tau = -1;
        int computed = 0;
        for (int tau = 1; tau <= max_tau; ++tau) {
            const float diff = sqdiff(recent, recent - tau, win);
            running_sum += diff;
            yin_buf_[tau] = (running_sum > 1e-9f) ? (diff * tau / running_sum) : 1.0f;
            computed = tau;

            if (tau < min_tau) continue;
            if (best_tau < 0) {
                if (yin_buf_[tau] < threshold_) best_tau = tau;
            } else if (yin_buf_[tau] < yin_buf_[best_tau]) {
                best_tau = tau; // still descending into the minimum
            } else {
                // Octave-up guard: on bridge pickups / fresh strings the 2nd harmonic can briefly
                // dip below threshold at T0/2. If we were tracking ~2*best_tau on the previous frame
                // and 2*best_tau is still strongly periodic, continue to 2*best_tau.
                if (prev_tau_ > 0.0f &&
                    std::fabs(static_cast<float>(best_tau) - 0.5f * prev_tau_) < 0.15f * prev_tau_ &&
                    best_tau * 2 + 2 <= max_tau) {
                    best_tau = -1; // keep searching toward prev_tau_
                } else {
                    break; // passed the local minimum; yin_buf_[best_tau + 1] is valid
                }
            }
        }

        // No value fell below threshold: global minimum over the fully computed range.
        if (best_tau == -1) {
            float min_val = 1.0f;
            for (int tau = min_tau; tau < max_tau; ++tau) {
                if (yin_buf_[tau] < min_val) {
                    min_val = yin_buf_[tau];
                    best_tau = tau;
                }
            }
            if (min_val > 0.40f) {
                return est; // not periodic enough
            }
        }
        (void)computed;

        // Right after an onset the analysis window is short. Accept a period only if the window
        // spans at least 1.5 periods; shorter windows give consistently biased (wrong-semitone) lags.
        if (max_tau < full_max_tau && best_tau * 3 > max_tau * 2) {
            return est;
        }

        // 4. Parabolic interpolation
        if (best_tau > min_tau && best_tau < max_tau) {
            float s0 = yin_buf_[best_tau - 1];
            float s1 = yin_buf_[best_tau];
            float s2 = yin_buf_[best_tau + 1];
            float denom = 2.0f * (2.0f * s1 - s2 - s0);
            float delta = (std::fabs(denom) > 1e-9f) ? (s2 - s0) / denom : 0.0f;
            float refined_tau = static_cast<float>(best_tau) + delta;

            float f0 = sample_rate / refined_tau;
            float midi = 69.0f + 12.0f * std::log2(f0 / 440.0f);

            if (midi >= 24.0f && midi <= 108.0f) {
                est.f0_hz = f0;
                est.midi_note = midi;
                est.confidence = std::clamp(1.0f - s1, 0.0f, 1.0f);
                est.is_valid = true;
                prev_tau_ = refined_tau;
            }
        }

        return est;
    }

    // sum_j (a[j]-b[j])^2 with independent accumulators so the compiler can vectorise.
    static float sqdiff(const float* a, const float* b, int n) {
        float acc[8] = {};
        int j = 0;
        for (; j + 8 <= n; j += 8) {
            for (int k = 0; k < 8; ++k) {
                const float d = a[j + k] - b[j + k];
                acc[k] += d * d;
            }
        }
        float tail = 0.0f;
        for (; j < n; ++j) {
            const float d = a[j] - b[j];
            tail += d * d;
        }
        return ((acc[0] + acc[1]) + (acc[2] + acc[3])) + ((acc[4] + acc[5]) + (acc[6] + acc[7])) + tail;
    }

    float threshold_{0.20f};
    alignas(64) std::array<float, kWinSize> lin_{};
    alignas(64) std::array<float, kWinSize> ring_{};
    alignas(64) std::array<float, kMaxTau> yin_buf_{};
    int write_pos_{0};
    int hop_counter_{0};
    int valid_history_{kWinSize};
    float prev_tau_{0.0f};
    PitchEstimate last_estimate_{};
};

} // namespace transducer::dsp
