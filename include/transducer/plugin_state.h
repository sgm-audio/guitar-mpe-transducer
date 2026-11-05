#pragma once

#include <transducer/dsp/transient_detector.h>
#include <transducer/dsp/yin_pitch_tracker.h>
#include <transducer/dsp/pitch_continuity.h>
#include <transducer/dsp/release_velocity_estimator.h>
#include <transducer/common/thread_check.h>
#include <cstdint>

namespace transducer {

struct PluginState {
    dsp::TransientDetector transient_detector{0.012f};
    dsp::YinPitchTracker pitch_tracker{0.20f};
    dsp::PitchContinuityFilter pitch_continuity{};
    dsp::ReleaseVelocityEstimator release_velocity{};
    common::ThreadCheck thread_check{};

    double sample_rate{48000.0};
    uint32_t min_frames{32};
    uint32_t max_frames{512};

    int32_t active_note_id{-1};
    int16_t active_key{-1};
    int32_t next_note_id{100};

    float pitch_bend_range{48.0f}; // Default 48 semitones MPE/CLAP standard

    void reset() {
        transient_detector.reset();
        pitch_tracker.reset();
        pitch_continuity.reset();
        release_velocity.reset();
        active_note_id = -1;
        active_key = -1;
    }
};

} // namespace transducer
