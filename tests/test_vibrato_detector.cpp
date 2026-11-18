#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/vibrato_detector.h>
#include <cmath>

TEST_CASE("VibratoDetector distinguishes steady pitch from 5Hz guitar finger vibrato", "[dsp][vibrato]") {
    using namespace transducer::dsp;
    VibratoDetector detector;
    constexpr float kHopRate = 750.0f; // 48000 / 64 ~ 750 updates per sec
    constexpr float kTwoPi = 6.28318530717958647692f;

    // 1. Steady unmodulated note (flat 60.0 MIDI)
    detector.reset();
    for (int i = 0; i < 300; ++i) {
        detector.process(60.0f);
    }
    REQUIRE(detector.depth_semitones() < 0.05f);
    REQUIRE(!detector.is_active());

    // 2. 5.5 Hz vibrato with +-0.5 semitones (1 semitone peak-to-peak)
    detector.reset();
    for (int i = 0; i < 300; ++i) {
        float pitch = 60.0f + 0.5f * std::sin(kTwoPi * 5.5f * i / kHopRate);
        detector.process(pitch);
    }
    REQUIRE(detector.is_active());
    REQUIRE_THAT(detector.rate_hz(), Catch::Matchers::WithinAbs(5.5f, 0.7f));
    REQUIRE_THAT(detector.depth_semitones(), Catch::Matchers::WithinAbs(0.5f, 0.15f));
}

TEST_CASE("VibratoDetector::prepare scales rate to arbitrary hop rates", "[dsp][vibrato]") {
    using namespace transducer::dsp;
    VibratoDetector detector;
    constexpr float kHopRate = 1500.0f; // 96 kHz / 64
    constexpr float kTwoPi = 6.28318530717958647692f;
    detector.prepare(kHopRate);
    for (int i = 0; i < 600; ++i) {
        detector.process(60.0f + 0.4f * std::sin(kTwoPi * 6.0f * static_cast<float>(i) / kHopRate));
    }
    REQUIRE(detector.is_active());
    REQUIRE_THAT(detector.rate_hz(), Catch::Matchers::WithinAbs(6.0f, 0.8f));
}
