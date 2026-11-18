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

#include <chrono>
#include <random>

TEST_CASE("YinPitchTracker is accurate across the guitar range", "[dsp][yin]") {
    constexpr float kSr = 48000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;
    for (float f0 : {82.41f, 110.0f, 196.0f, 329.63f, 659.26f, 1318.5f}) {
        transducer::dsp::YinPitchTracker tracker;
        float midi = 0.0f;
        bool ok = false;
        for (int i = 0; i < 4800; ++i) {
            float ph = kTwoPi * f0 * static_cast<float>(i) / kSr;
            float x = 0.5f * (std::sin(ph) + 0.4f * std::sin(2 * ph) + 0.2f * std::sin(3 * ph));
            auto r = tracker.processSample(x, kSr);
            if (r && r->is_valid) { midi = r->midi_note; ok = true; }
        }
        float expect = 69.0f + 12.0f * std::log2(f0 / 440.0f);
        INFO("f0=" << f0);
        REQUIRE(ok);
        REQUIRE_THAT(midi, Catch::Matchers::WithinAbs(expect, 0.15f));
    }
}

TEST_CASE("YinPitchTracker worst-case hop cost stays well inside the hop period", "[dsp][yin][perf]") {
    // Aperiodic noise defeats the early exit: every lag is computed (worst case).
    constexpr float kSr = 48000.0f;
    transducer::dsp::YinPitchTracker tracker;
    std::mt19937 rng(1);
    std::uniform_real_distribution<float> d(-0.5f, 0.5f);
    for (int i = 0; i < 4096; ++i) tracker.processSample(d(rng), kSr); // warm up

    constexpr int kHops = 200;
    double worst_us = 0.0;
    for (int h = 0; h < kHops; ++h) {
        for (int i = 0; i < transducer::dsp::YinPitchTracker::kHopSize - 1; ++i) {
            tracker.processSample(d(rng), kSr);
        }
        auto t0 = std::chrono::steady_clock::now();
        tracker.processSample(d(rng), kSr); // the sample that triggers computePitch
        auto t1 = std::chrono::steady_clock::now();
        worst_us = std::max(worst_us, std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    const double hop_us = 1e6 * transducer::dsp::YinPitchTracker::kHopSize / kSr; // 1333 us
    INFO("worst hop = " << worst_us << " us of " << hop_us << " us");
    REQUIRE(worst_us < hop_us); // generous: unoptimised/CI machines vary
}
