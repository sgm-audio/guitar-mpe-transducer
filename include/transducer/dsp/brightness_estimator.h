#pragma once

#include <cmath>
#include <algorithm>

namespace transducer::dsp {

class BrightnessEstimator {
public:
    BrightnessEstimator() = default;

    void reset() {
        prev_sample_ = 0.0f;
        hp_energy_ = 0.0f;
        total_energy_ = 0.0f;
        brightness_ = 0.1f;
    }

    // Call per sample; returns smoothed brightness [0.0, 1.0]
    float process(float sample, float sample_rate = 48000.0f) {
        float diff = sample - prev_sample_;
        prev_sample_ = sample;

        float diff_sq = diff * diff;
        float total_sq = sample * sample;

        hp_energy_ = 0.95f * hp_energy_ + 0.05f * diff_sq;
        total_energy_ = 0.95f * total_energy_ + 0.05f * total_sq;

        if (total_energy_ > 1e-6f) {
            // Rice formula for spectral centroid proxy:
            // f_centroid ~ (fs / (2*pi)) * sqrt( E(diff^2) / E(x^2) )
            float ratio = hp_energy_ / total_energy_;
            float approx_freq = (sample_rate / 6.2831853f) * std::sqrt(ratio);

            // Normalize: 100 Hz (warm) -> 0.025, 4000 Hz (cutting harmonic) -> 1.0
            float norm = std::clamp(approx_freq / 3500.0f, 0.0f, 1.0f);
            brightness_ = 0.90f * brightness_ + 0.10f * norm;
        }

        return brightness_;
    }

    float brightness() const { return brightness_; }

private:
    float prev_sample_{0.0f};
    float hp_energy_{0.0f};
    float total_energy_{0.0f};
    float brightness_{0.1f};
};

} // namespace transducer::dsp
