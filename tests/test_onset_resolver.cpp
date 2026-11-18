#include <catch2/catch_test_macros.hpp>
#include <transducer/dsp/onset_resolver.h>

using transducer::dsp::OnsetResolver;
using transducer::dsp::ResolvedNote;

TEST_CASE("OnsetResolver waits for velocity and two stable pitch frames", "[dsp][onset]") {
    OnsetResolver r;
    r.prepare(48000.0f);
    ResolvedNote n{};

    REQUIRE_FALSE(r.on_pitch(true, 60.0f, 0.95f, n)); // nothing pending

    r.on_onset();
    REQUIRE_FALSE(r.on_pitch(true, 60.0f, 0.95f, n));
    REQUIRE_FALSE(r.on_pitch(true, 60.0f, 0.95f, n)); // stable but velocity unknown
    r.on_velocity(0.7f);
    REQUIRE(r.on_pitch(true, 60.2f, 0.95f, n));
    REQUIRE(n.key == 60);
    REQUIRE(n.velocity == 0.7f);
    REQUIRE_FALSE(r.pending());
}

TEST_CASE("OnsetResolver rejects unstable or low-confidence pitch and times out", "[dsp][onset]") {
    OnsetResolver r;
    r.prepare(48000.0f);
    ResolvedNote n{};
    r.on_onset();
    r.on_velocity(0.5f);
    REQUIRE_FALSE(r.on_pitch(true, 50.0f, 0.95f, n));
    REQUIRE_FALSE(r.on_pitch(true, 62.0f, 0.95f, n)); // jumped
    REQUIRE_FALSE(r.on_pitch(true, 62.0f, 0.3f, n));  // unconfident resets stability
    REQUIRE_FALSE(r.on_pitch(true, 62.0f, 0.95f, n));
    REQUIRE(r.on_pitch(true, 62.1f, 0.95f, n));
    REQUIRE(n.key == 62);

    r.on_onset();
    bool timed_out = false;
    for (int i = 0; i < 48000 && !timed_out; ++i) timed_out = r.tick();
    REQUIRE(timed_out);
    REQUIRE_FALSE(r.pending());
}
