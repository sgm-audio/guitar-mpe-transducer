#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/pitch_continuity.h>

TEST_CASE("PitchContinuityFilter suppresses transient octave jumps", "[dsp][continuity]") {
    using namespace transducer::dsp;
    PitchContinuityFilter filter;

    // Establish fundamental at E2 (MIDI 40.0)
    for (int i = 0; i < 5; ++i) {
        float out = filter.process(40.0f, 0.95f, true);
        REQUIRE_THAT(out, Catch::Matchers::WithinAbs(40.0f, 0.05f));
    }

    // 1. Spurious single-frame octave-up glitch (e.g. 2nd harmonic picked up by YIN, MIDI 52.0)
    float glitched = filter.process(52.0f, 0.70f, true);
    // Should suppress the jump and stay near 40.0
    REQUIRE_THAT(glitched, Catch::Matchers::WithinAbs(40.0f, 0.5f));

    // Next frame fundamental returns
    float restored = filter.process(40.0f, 0.95f, true);
    REQUIRE_THAT(restored, Catch::Matchers::WithinAbs(40.0f, 0.05f));

    // 2. Normal fret transitions (40.0 -> 42.0 -> 44.0) must pass cleanly
    float note_fret2 = filter.process(42.0f, 0.95f, true);
    REQUIRE_THAT(note_fret2, Catch::Matchers::WithinAbs(42.0f, 0.5f));
    float note_fret4 = filter.process(44.0f, 0.95f, true);
    REQUIRE_THAT(note_fret4, Catch::Matchers::WithinAbs(44.0f, 0.5f));
}
