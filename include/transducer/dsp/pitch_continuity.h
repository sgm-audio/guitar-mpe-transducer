#pragma once

#include <cmath>
#include <algorithm>

namespace transducer::dsp {

class PitchContinuityFilter {
public:
    explicit PitchContinuityFilter(float max_step_semitones = 3.5f)
        : max_step_(max_step_semitones) {}

    void reset() {
        last_pitch_ = 0.0f;
        has_history_ = false;
        jump_candidate_pitch_ = 0.0f;
        jump_consecutive_frames_ = 0;
    }

    float process(float raw_midi_pitch, float confidence, bool is_valid) {
        if (!is_valid) {
            return last_pitch_;
        }

        if (!has_history_) {
            last_pitch_ = raw_midi_pitch;
            has_history_ = true;
            return raw_midi_pitch;
        }

        float diff = raw_midi_pitch - last_pitch_;
        float abs_diff = std::fabs(diff);

        // Check if delta is an octave jump: close to 12.0 or 24.0 semitones (+-1.2 semitones)
        bool is_octave_jump = (std::fabs(abs_diff - 12.0f) < 1.2f) || (std::fabs(abs_diff - 24.0f) < 1.5f);

        if (is_octave_jump && confidence < 0.98f) {
            // Check if this jump is sustained over consecutive frames
            if (std::fabs(raw_midi_pitch - jump_candidate_pitch_) < 0.5f) {
                ++jump_consecutive_frames_;
            } else {
                jump_candidate_pitch_ = raw_midi_pitch;
                jump_consecutive_frames_ = 1;
            }

            // Require 3 sustained frames to accept an octave jump
            if (jump_consecutive_frames_ >= 3) {
                last_pitch_ = raw_midi_pitch;
                return raw_midi_pitch;
            }

            // Otherwise, hold the last reliable fundamental
            return last_pitch_;
        }

        jump_consecutive_frames_ = 0;

        // Smooth legitimate pitch changes (e.g. fret transitions or natural bend)
        last_pitch_ = 0.85f * last_pitch_ + 0.15f * raw_midi_pitch;
        // If difference was larger than half a step, follow faster
        if (abs_diff > 1.0f) {
            last_pitch_ = raw_midi_pitch;
        }

        return last_pitch_;
    }

    float last_pitch() const { return last_pitch_; }

private:
    float max_step_{3.5f};
    float last_pitch_{0.0f};
    bool has_history_{false};
    float jump_candidate_pitch_{0.0f};
    int jump_consecutive_frames_{0};
};

} // namespace transducer::dsp
