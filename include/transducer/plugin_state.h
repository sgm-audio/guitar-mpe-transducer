#pragma once

#include <transducer/dsp/transient_detector.h>
#include <transducer/dsp/yin_pitch_tracker.h>
#include <transducer/dsp/pitch_continuity.h>
#include <transducer/dsp/release_velocity_estimator.h>
#include <transducer/dsp/brightness_estimator.h>
#include <transducer/dsp/articulation_classifier.h>
#include <transducer/dsp/vibrato_detector.h>
#include <transducer/voice/voice_allocator.h>
#include <transducer/common/thread_check.h>
#include <cstdint>

namespace transducer {

struct PluginState {
    dsp::TransientDetector transient_detector{0.012f};
    dsp::YinPitchTracker pitch_tracker{0.20f};
    dsp::PitchContinuityFilter pitch_continuity{};
    dsp::ReleaseVelocityEstimator release_velocity{};
    dsp::BrightnessEstimator brightness_estimator{};
    dsp::ArticulationClassifier articulation_classifier{};
    dsp::VibratoDetector vibrato_detector{};
    voice::VoiceAllocator voice_allocator{};
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
        brightness_estimator.reset();
        articulation_classifier.reset();
        vibrato_detector.reset();
        voice_allocator.reset();
        active_note_id = -1;
        active_key = -1;
    }
};

} // namespace transducer
