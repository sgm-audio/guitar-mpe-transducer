#include <cmath>
#include <catch2/catch_test_macros.hpp>
#include <transducer/dsp/articulation_classifier.h>

TEST_CASE("ArticulationClassifier categorizes open pluck, palm mute, slow attack and harmonic", "[dsp][articulation]") {
    using namespace transducer::dsp;
    constexpr float kSr = 48000.0f;
    constexpr float kTwoPi = 6.28318530718f;
    ArticulationClassifier c;
    c.prepare(kSr);

    auto feed_tone = [&](float f0, float overtone, float tau, bool slow_attack = false) {
        c.reset();
        for (int i = 0; i < 2400; ++i) {
            float t = static_cast<float>(i) / kSr;
            float env = slow_attack ? std::min(t / 0.040f, 1.0f) : std::exp(-t / tau);
            float ph = kTwoPi * f0 * t;
            float x = 0.5f * env * (std::sin(ph) + overtone * (std::sin(2 * ph) + 0.5f * std::sin(3 * ph)));
            c.feed(x);
        }
    };

    // 1. Palm mute: decays > 12 dB within 45 ms
    feed_tone(110.0f, 0.5f, 0.010f);
    REQUIRE(c.ready());
    REQUIRE(c.classify(110.0f) == ArticulationType::kPalmMute);

    // 2. Open pluck: overtone-rich, sustains
    feed_tone(110.0f, 0.6f, 0.400f);
    REQUIRE(c.classify(110.0f) == ArticulationType::kOpenPluck);

    // 3. Slow attack: peak arrives in frame 2, must NOT be misclassified as palm mute
    feed_tone(110.0f, 0.6f, 1.0f, true);
    REQUIRE(c.classify(110.0f) == ArticulationType::kOpenPluck);

    // 4. Artificial harmonic: 12th-fret harmonic (~659 Hz), near-pure sine
    feed_tone(659.26f, 0.0f, 0.400f);
    REQUIRE(c.classify(659.26f) == ArticulationType::kHarmonicTap);
}
