#pragma once

#include <algorithm>
#include <cmath>

namespace transducer::dsp {

// Brightness proxy in [0, 1] from the RMS frequency of the signal (Rice formula):
//   f_rms ~ (fs / 2pi) * sqrt( E[(x[n]-x[n-1])^2] / E[x^2] )
// Note this is an RMS frequency (weights harmonics by f^2), not a true spectral centroid; it is
// monotonic with "pick-near-bridge" brightness, which is what the note expression needs.
//
// Energies are averaged over ~10 ms (>= one period of the lowest string, so no per-cycle
// zero-crossing spikes), the result is smoothed over ~5 ms, and it is frozen while the signal is
// below the energy floor so silence/decay noise does not move it.
class BrightnessEstimator {
public:
    static constexpr float kEnergyTauSeconds = 0.010f;
    static constexpr float kOutputTauSeconds = 0.005f;
    static constexpr float kEnergyFloor = 1e-6f; // ~ -60 dB mean square

    BrightnessEstimator() = default;

    void reset() {
        prev_sample_ = 0.0f;
        hp_energy_ = 0.0f;
        total_energy_ = 0.0f;
        brightness_ = 0.1f;
    }

    // Call per sample; returns smoothed brightness [0.0, 1.0]
    float process(float sample, float sample_rate = 48000.0f) {
        if (sample_rate != coeff_rate_) {
            coeff_rate_ = sample_rate;
            energy_a_ = 1.0f - std::exp(-1.0f / (kEnergyTauSeconds * sample_rate));
            output_a_ = 1.0f - std::exp(-1.0f / (kOutputTauSeconds * sample_rate));
        }

        const float diff = sample - prev_sample_;
        prev_sample_ = sample;

        hp_energy_ += energy_a_ * (diff * diff - hp_energy_);
        total_energy_ += energy_a_ * (sample * sample - total_energy_);
        if (hp_energy_ < 1e-20f) hp_energy_ = 0.0f;       // denormal guards
        if (total_energy_ < 1e-20f) total_energy_ = 0.0f;

        if (total_energy_ > kEnergyFloor) {
            const float ratio = hp_energy_ / total_energy_;
            const float approx_freq = (sample_rate / 6.2831853f) * std::sqrt(ratio);
            // Normalize: 100 Hz (warm) -> ~0.03, 3500 Hz and above -> 1.0
            const float norm = std::clamp(approx_freq / 3500.0f, 0.0f, 1.0f);
            brightness_ += output_a_ * (norm - brightness_);
        }

        return brightness_;
    }

    float brightness() const { return brightness_; }

private:
    float prev_sample_{0.0f};
    float hp_energy_{0.0f};
    float total_energy_{0.0f};
    float brightness_{0.1f};
    float coeff_rate_{0.0f};
    float energy_a_{0.0f};
    float output_a_{0.0f};
};

} // namespace transducer::dsp
