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
    static constexpr int kWinSize = 2048;
    static constexpr int kMaxTau = 1024;
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
        last_estimate_ = {};
    }

    std::optional<PitchEstimate> processSample(float sample, float sample_rate) {
        ring_[write_pos_] = sample;
        write_pos_ = (write_pos_ + 1) & (kWinSize - 1);

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
        for (int i = 0; i < kMaxTau; ++i) {
            float s = ring_[(write_pos_ - 1 - i) & (kWinSize - 1)];
            energy += s * s;
        }
        if (energy < 0.001f) {
            return est;
        }

        // 1. Difference function & 2. Cumulative Mean Normalized Difference Function (CMNDF)
        yin_buf_[0] = 1.0f;
        float running_sum = 0.0f;

        // Min lag = 18 (~2666Hz @ 48kHz), Max lag = 900 (~53Hz @ 48kHz)
        const int min_tau = std::max(10, static_cast<int>(sample_rate / 3000.0f));
        const int max_tau = std::min(kMaxTau - 2, static_cast<int>(sample_rate / 50.0f));

        for (int tau = 1; tau <= max_tau; ++tau) {
            float diff = 0.0f;
            for (int i = 0; i < max_tau; ++i) {
                float a = ring_[(write_pos_ - 1 - i) & (kWinSize - 1)];
                float b = ring_[(write_pos_ - 1 - i - tau) & (kWinSize - 1)];
                float delta = a - b;
                diff += delta * delta;
            }
            running_sum += diff;
            yin_buf_[tau] = (running_sum > 1e-9f) ? (diff * tau / running_sum) : 1.0f;
        }

        // 3. Absolute threshold search
        int best_tau = -1;
        for (int tau = min_tau; tau < max_tau; ++tau) {
            if (yin_buf_[tau] < threshold_) {
                while (tau + 1 < max_tau && yin_buf_[tau + 1] < yin_buf_[tau]) {
                    ++tau;
                }
                best_tau = tau;
                break;
            }
        }

        // If no value fell below threshold, find global minimum
        if (best_tau == -1) {
            float min_val = 1.0f;
            for (int tau = min_tau; tau < max_tau; ++tau) {
                if (yin_buf_[tau] < min_val) {
                    min_val = yin_buf_[tau];
                    best_tau = tau;
                }
            }
            if (min_val > 0.40f) {
                // Not periodic enough
                return est;
            }
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
            }
        }

        return est;
    }

    float threshold_{0.20f};
    alignas(64) std::array<float, kWinSize> ring_{};
    alignas(64) std::array<float, kMaxTau> yin_buf_{};
    int write_pos_{0};
    int hop_counter_{0};
    PitchEstimate last_estimate_{};
};

} // namespace transducer::dsp
