#pragma once

#include <cmath>
#include <algorithm>

namespace transducer::dsp {

class ReleaseVelocityEstimator {
public:
    ReleaseVelocityEstimator() = default;

    void reset() {
        prev_envelope_ = 0.0f;
        release_velocity_ = 0.1f;
    }

    // Call on each envelope update; returns estimated release velocity [0.05, 1.0]
    float process(float current_envelope) {
        float drop = prev_envelope_ - current_envelope;
        prev_envelope_ = current_envelope;

        if (drop > 0.0f) {
            // Steep drop (e.g. 0.4 in one hop) maps directly to high release velocity
            float instant_vel = std::clamp(drop * 2.5f, 0.05f, 1.0f);
            if (instant_vel > release_velocity_) {
                release_velocity_ = instant_vel;
            } else {
                release_velocity_ = 0.8f * release_velocity_ + 0.2f * instant_vel;
            }
        } else {
            // Sustained or rising energy maintains baseline low release velocity
            release_velocity_ = 0.95f * release_velocity_ + 0.05f * 0.1f;
        }

        return release_velocity_;
    }

    float release_velocity() const { return release_velocity_; }

private:
    float prev_envelope_{0.0f};
    float release_velocity_{0.1f};
};

} // namespace transducer::dsp
