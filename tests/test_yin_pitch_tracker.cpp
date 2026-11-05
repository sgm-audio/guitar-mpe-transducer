#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/yin_pitch_tracker.h>
#include <cmath>

TEST_CASE("YinPitchTracker tracks 440Hz sine wave pitch accurately", "[dsp][yin]") {
    transducer::dsp::YinPitchTracker tracker;
    constexpr float kSampleRate = 48000.0f;
    constexpr float kTargetFreq = 440.0f; // A4, MIDI 69
    constexpr float kTwoPi = 6.28318530717958647692f;

    bool detected = false;
    float detected_midi = 0.0f;

    // Feed ~50ms of 440Hz sine
    const int num_samples = static_cast<int>(kSampleRate * 0.05f);
    for (int i = 0; i < num_samples; ++i) {
        float sample = 0.7f * std::sin(kTwoPi * kTargetFreq * i / kSampleRate);
        auto res = tracker.processSample(sample, kSampleRate);
        if (res.has_value() && res->is_valid) {
            detected = true;
            detected_midi = res->midi_note;
        }
    }

    REQUIRE(detected);
    REQUIRE_THAT(detected_midi, Catch::Matchers::WithinAbs(69.0f, 0.15f));
}

TEST_CASE("YinPitchTracker tracks low E2 (82.4Hz) guitar fundamental", "[dsp][yin]") {
    transducer::dsp::YinPitchTracker tracker;
    constexpr float kSampleRate = 48000.0f;
    constexpr float kTargetFreq = 82.4f; // E2, MIDI 40
    constexpr float kTwoPi = 6.28318530717958647692f;

    bool detected = false;
    float detected_midi = 0.0f;

    // Feed ~60ms of 82.4Hz sine
    const int num_samples = static_cast<int>(kSampleRate * 0.06f);
    for (int i = 0; i < num_samples; ++i) {
        float sample = 0.7f * std::sin(kTwoPi * kTargetFreq * i / kSampleRate);
        auto res = tracker.processSample(sample, kSampleRate);
        if (res.has_value() && res->is_valid) {
            detected = true;
            detected_midi = res->midi_note;
        }
    }

    REQUIRE(detected);
    REQUIRE_THAT(detected_midi, Catch::Matchers::WithinAbs(40.0f, 0.25f));
}
