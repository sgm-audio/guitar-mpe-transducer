#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/release_velocity_estimator.h>
#include <transducer/midi2/ump_builder.h>
#include <transducer/clap/event_emitter.h>

TEST_CASE("ReleaseVelocityEstimator distinguishes sudden choke from natural ringout", "[dsp][release]") {
    using namespace transducer::dsp;
    ReleaseVelocityEstimator est;

    // 1. Natural ringout: slow decay
    est.reset();
    float natural_rel_vel = 0.0f;
    for (int i = 0; i < 100; ++i) {
        float env = 0.5f * std::exp(-0.01f * i);
        natural_rel_vel = est.process(env);
    }
    // Natural decay should produce gentle release velocity (< 0.3)
    REQUIRE(natural_rel_vel < 0.3f);

    // 2. Sudden choke / palm mute: abrupt drop from 0.5 to 0.001 in 2 frames
    est.reset();
    est.process(0.5f);
    est.process(0.48f);
    float choke_rel_vel = est.process(0.001f);
    // Sudden mute should produce high release velocity (> 0.7)
    REQUIRE(choke_rel_vel > 0.7f);
}

TEST_CASE("UmpBuilder encodes release velocity in Note-Off UMP", "[midi2][ump]") {
    using namespace transducer::midi2;
    auto ump = UmpBuilder::makeNoteOff(0, 0, 60, 0.75f);
    REQUIRE(((ump[0] >> 20) & 0xF) == 0x8); // Note Off status
    uint16_t vel16 = static_cast<uint16_t>(ump[1] >> 16);
    // 0.75 * 65535 ~ 49151 (0xBFFF)
    REQUIRE(vel16 >= 0xBF00);
    REQUIRE(vel16 <= 0xC100);
}

TEST_CASE("ReleaseVelocityEstimator::update detects >=30dB mute in 10ms and scales with dB/ms", "[dsp][release]") {
    using namespace transducer::dsp;
    ReleaseVelocityEstimator est;
    est.prepare(750.0f); // 48 kHz / 64

    // Natural decay (~0.05 dB/ms): never muted, low release velocity
    float nat = 0.0f;
    for (int i = 0; i < 60; ++i) {
        nat = est.update(0.4f * std::exp(-0.004f * static_cast<float>(i)));
        REQUIRE_FALSE(est.muted());
    }
    REQUIRE(nat < 0.15f);

    // Sudden choke: 0.4 -> 0.005 (~38 dB) over 4 hops (~5.3 ms)
    est.reset();
    for (int i = 0; i < 12; ++i) est.update(0.4f);
    float vel = 0.0f;
    for (int i = 0; i < 5; ++i) {
        vel = est.update(0.4f * std::pow(0.25f, static_cast<float>(i + 1)));
    }
    REQUIRE(est.muted());
    REQUIRE(vel > 0.7f);
}
