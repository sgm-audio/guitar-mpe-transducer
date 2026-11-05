#include <catch2/catch_test_macros.hpp>
#include <transducer/dsp/articulation_classifier.h>

TEST_CASE("ArticulationClassifier categorizes open pluck vs palm mute", "[dsp][articulation]") {
    using namespace transducer::dsp;
    ArticulationClassifier classifier;

    // 1. Palm mute profile: sharp transient peak followed by immediate decay to < 10% within 15ms
    classifier.reset();
    for (int i = 0; i < 200; ++i) {
        float env = (i < 5) ? (0.2f * i) : (1.0f * std::exp(-0.2f * (i - 5)));
        classifier.feedSample(env, 0.4f);
    }
    REQUIRE(classifier.classify() == ArticulationType::kPalmMute);

    // 2. Open pluck profile: sharp transient followed by sustained ringing decay
    classifier.reset();
    for (int i = 0; i < 200; ++i) {
        float env = (i < 5) ? (0.2f * i) : (1.0f * std::exp(-0.005f * (i - 5)));
        classifier.feedSample(env, 0.4f);
    }
    REQUIRE(classifier.classify() == ArticulationType::kOpenPluck);
}
