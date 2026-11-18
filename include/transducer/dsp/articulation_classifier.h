#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace transducer::dsp {

enum class ArticulationType : uint8_t {
    kOpenPluck = 0,
    kPalmMute = 1,
    kHarmonicTap = 2
};

// Classifies one pluck, starting at reset() (call at every onset), from raw samples.
//
//  * Palm mute: peak amplitude is measured in three consecutive 15 ms frames after the onset.
//    The last frame (30-45 ms) being >= 12 dB below the loudest earlier frame is a mute: a
//    ringing string loses ~1-3 dB in that time, so open notes and slow-attack notes (peak in the
//    last frame -> ratio >= 1) never qualify.
//  * Artificial harmonic: with the YIN f0 supplied, Goertzel (Hann-windowed) energy at 2f0..4f0
//    versus f0 over the first 2048 samples. A near-sinusoid (< 3 % overtone energy) is a
//    harmonic; fretted/open strings are overtone-rich. Needs >= ~4 cycles in the window
//    (f0 >= ~94 Hz @ 48 kHz); below that harmonics are not classified.
//
// `classify()` returns kOpenPluck until `ready()` (45 ms after the onset), then the cached
// result. The Goertzel pass runs once per onset, not per sample.
class ArticulationClassifier {
public:
    static constexpr size_t kBufLen = 2048;
    static constexpr float kFrameSeconds = 0.015f;
    static constexpr float kMuteRatio = 0.25f;           // -12 dB
    static constexpr float kHarmonicOvertoneRatio = 0.03f;

    ArticulationClassifier() { buf_.fill(0.0f); }

    void prepare(float sample_rate) noexcept {
        sample_rate_ = sample_rate;
        frame_len_ = std::max(1, static_cast<int>(kFrameSeconds * sample_rate));
        reset();
    }

    void reset() noexcept {
        count_ = 0;
        frame_idx_ = 0;
        frame_count_ = 0;
        frame_peak_ = {};
        cur_peak_ = 0.0f;
        done_ = false;
        result_ = ArticulationType::kOpenPluck;
    }

    void feed(float sample) noexcept {
        if (count_ < static_cast<int>(kBufLen)) buf_[static_cast<size_t>(count_)] = sample;
        ++count_;
        if (frame_idx_ >= 3) return;
        cur_peak_ = std::max(cur_peak_, std::fabs(sample));
        if (++frame_count_ >= frame_len_) {
            frame_peak_[static_cast<size_t>(frame_idx_++)] = cur_peak_;
            cur_peak_ = 0.0f;
            frame_count_ = 0;
        }
    }

    bool ready() const noexcept { return frame_idx_ >= 3 && count_ >= static_cast<int>(kBufLen); }

    ArticulationType classify(float f0_hz = 0.0f) noexcept {
        if (!ready()) return ArticulationType::kOpenPluck;
        if (done_) return result_;
        done_ = true;

        const float loud = std::max(frame_peak_[0], frame_peak_[1]);
        if (loud < 1e-4f) {
            result_ = ArticulationType::kOpenPluck;
        } else if (frame_peak_[2] < loud * kMuteRatio) {
            result_ = ArticulationType::kPalmMute;
        } else if (is_harmonic(f0_hz)) {
            result_ = ArticulationType::kHarmonicTap;
        } else {
            result_ = ArticulationType::kOpenPluck;
        }
        return result_;
    }

private:
    bool is_harmonic(float f0) const noexcept {
        const float min_f0 = 4.0f * sample_rate_ / static_cast<float>(kBufLen);
        if (f0 < min_f0 || 4.0f * f0 >= 0.5f * sample_rate_) return false;
        const float e1 = goertzel_power(f0);
        if (e1 <= 0.0f) return false;
        const float eh = goertzel_power(2.0f * f0) + goertzel_power(3.0f * f0) + goertzel_power(4.0f * f0);
        return eh / e1 < kHarmonicOvertoneRatio;
    }

    float goertzel_power(float freq) const noexcept {
        const double w = 6.283185307179586 * static_cast<double>(freq) / static_cast<double>(sample_rate_);
        const double coeff = 2.0 * std::cos(w);
        double s1 = 0.0, s2 = 0.0;
        const double n_inv = 6.283185307179586 / static_cast<double>(kBufLen - 1);
        for (size_t i = 0; i < kBufLen; ++i) {
            const double hann = 0.5 - 0.5 * std::cos(n_inv * static_cast<double>(i));
            const double s0 = static_cast<double>(buf_[i]) * hann + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        return static_cast<float>(s1 * s1 + s2 * s2 - coeff * s1 * s2);
    }

    float sample_rate_{48000.0f};
    int frame_len_{720};
    std::array<float, kBufLen> buf_{};
    std::array<float, 3> frame_peak_{};
    int count_{0};
    int frame_idx_{0};
    int frame_count_{0};
    float cur_peak_{0.0f};
    bool done_{false};
    ArticulationType result_{ArticulationType::kOpenPluck};
};

} // namespace transducer::dsp
