#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/transient_detector.h>
#include <vector>

TEST_CASE("TransientDetector detects pluck attack and ignores silence", "[dsp][transient]") {
    transducer::dsp::TransientDetector detector;

    // 1. Process 1000 samples of silence
    for (int i = 0; i < 1000; ++i) {
        auto res = detector.process(0.0f);
        REQUIRE(!res.onset_detected);
    }

    // 2. Feed sudden pluck transient (sharp jump)
    bool detected = false;
    float onset_vel = 0.0f;
    for (int i = 0; i < 64; ++i) {
        float sample = (i == 0) ? 0.8f : (0.8f * std::exp(-0.05f * i));
        auto res = detector.process(sample);
        if (res.onset_detected) {
            detected = true;
            onset_vel = res.velocity;
            break;
        }
    }
    REQUIRE(detected);
    REQUIRE(onset_vel > 0.5f);
}
