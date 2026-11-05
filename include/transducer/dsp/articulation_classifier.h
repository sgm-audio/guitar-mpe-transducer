#pragma once

#include <cstdint>
#include <algorithm>

namespace transducer::dsp {

enum class ArticulationType : uint8_t {
    kOpenPluck = 0,
    kPalmMute = 1,
    kHarmonicTap = 2
};

class ArticulationClassifier {
public:
    ArticulationClassifier() = default;

    void reset() {
        peak_env_ = 0.0f;
        sample_count_ = 0;
        env_at_window_ = 0.0f;
        avg_brightness_ = 0.0f;
    }

    void feedSample(float envelope, float brightness) {
        if (envelope > peak_env_) {
            peak_env_ = envelope;
        }

        avg_brightness_ = 0.90f * avg_brightness_ + 0.10f * brightness;

        if (++sample_count_ == 50) { // ~1ms after onset
            env_at_window_ = envelope;
        }
    }

    ArticulationType classify() const {
        if (peak_env_ < 1e-4f) {
            return ArticulationType::kOpenPluck;
        }

        // If harmonic ratio is very high and peak is modest -> artificial harmonic
        if (avg_brightness_ > 0.85f && peak_env_ < 0.4f) {
            return ArticulationType::kHarmonicTap;
        }

        // If energy decayed to less than 25% of peak within 50 samples -> palm mute
        float decay_ratio = env_at_window_ / peak_env_;
        if (decay_ratio < 0.25f) {
            return ArticulationType::kPalmMute;
        }

        return ArticulationType::kOpenPluck;
    }

private:
    float peak_env_{0.0f};
    int sample_count_{0};
    float env_at_window_{0.0f};
    float avg_brightness_{0.0f};
};

} // namespace transducer::dsp
