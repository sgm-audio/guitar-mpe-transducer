#pragma once

#include <array>
#include <cmath>
#include <algorithm>

namespace transducer::dsp {

class VibratoDetector {
public:
    static constexpr size_t kHistorySize = 128; // ~170ms at 750Hz hop rate

    VibratoDetector(float sample_rate = 750.0f)
        : hop_rate_(sample_rate) {
        history_.fill(0.0f);
    }

    void reset() {
        history_.fill(0.0f);
        write_pos_ = 0;
        count_ = 0;
        mean_pitch_ = 0.0f;
        depth_semitones_ = 0.0f;
        rate_hz_ = 0.0f;
        is_active_ = false;
    }

    void process(float continuous_midi_pitch) {
        history_[write_pos_] = continuous_midi_pitch;
        write_pos_ = (write_pos_ + 1) % kHistorySize;
        if (count_ < kHistorySize) {
            ++count_;
            return;
        }

        // Calculate mean pitch over window
        float sum = 0.0f;
        float min_p = 1000.0f;
        float max_p = -1000.0f;
        for (size_t i = 0; i < kHistorySize; ++i) {
            float p = history_[i];
            sum += p;
            if (p < min_p) min_p = p;
            if (p > max_p) max_p = p;
        }
        mean_pitch_ = sum / static_cast<float>(kHistorySize);

        float peak_to_peak = max_p - min_p;
        depth_semitones_ = peak_to_peak * 0.5f;

        // Count zero-crossings around mean
        int zero_crossings = 0;
        for (size_t i = 1; i < kHistorySize; ++i) {
            size_t curr_idx = (write_pos_ + i) % kHistorySize;
            size_t prev_idx = (write_pos_ + i - 1) % kHistorySize;
            float a = history_[prev_idx] - mean_pitch_;
            float b = history_[curr_idx] - mean_pitch_;
            if ((a < 0.0f && b > 0.0f) || (a > 0.0f && b < 0.0f)) {
                ++zero_crossings;
            }
        }

        // 2 zero crossings per cycle: frequency = (zero_crossings / 2) / duration_seconds
        float duration_sec = static_cast<float>(kHistorySize) / hop_rate_;
        rate_hz_ = (static_cast<float>(zero_crossings) * 0.5f) / duration_sec;

        // Typical human finger vibrato on guitar is 3.5Hz to 8.5Hz, depth > 0.15 semitones
        is_active_ = (depth_semitones_ > 0.15f && rate_hz_ >= 3.0f && rate_hz_ <= 9.0f);
    }

    bool is_active() const { return is_active_; }
    float depth_semitones() const { return depth_semitones_; }
    float rate_hz() const { return rate_hz_; }
    float mean_pitch() const { return mean_pitch_; }

private:
    float hop_rate_{750.0f};
    std::array<float, kHistorySize> history_{};
    size_t write_pos_{0};
    size_t count_{0};

    float mean_pitch_{0.0f};
    float depth_semitones_{0.0f};
    float rate_hz_{0.0f};
    bool is_active_{false};
};

} // namespace transducer::dsp
