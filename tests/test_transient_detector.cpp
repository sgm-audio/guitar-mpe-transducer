#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/transient_detector.h>
#include <cmath>
#include <vector>

namespace {
constexpr float kSr = 48000.0f;
constexpr float kTwoPi = 6.28318530718f;

struct Onset { int sample; float final_velocity; bool ready; };

// Feed a signal, return onset list (final velocity filled when velocity_ready arrives).
std::vector<Onset> run(transducer::dsp::TransientDetector& d, const std::vector<float>& x) {
    std::vector<Onset> out;
    for (size_t i = 0; i < x.size(); ++i) {
        auto r = d.process(x[i]);
        if (r.onset_detected) out.push_back({static_cast<int>(i), 0.0f, false});
        if (r.velocity_ready && !out.empty()) {
            out.back().final_velocity = r.final_velocity;
            out.back().ready = true;
        }
    }
    return out;
}

// Plucked tone: 1 ms linear attack, exponential decay (tau seconds), added to `buf` at `start`.
void add_pluck(std::vector<float>& buf, int start, float f0, float amp, float tau) {
    for (size_t i = start; i < buf.size(); ++i) {
        float t = static_cast<float>(i - start) / kSr;
        float att = std::min(t / 0.001f, 1.0f);
        float ph = kTwoPi * f0 * t;
        buf[i] += amp * att * std::exp(-t / tau) *
                  (std::sin(ph) + 0.4f * std::sin(2 * ph) + 0.2f * std::sin(3 * ph)) / 1.6f;
    }
}
} // namespace

TEST_CASE("TransientDetector detects pluck attack and ignores silence", "[dsp][transient]") {
    transducer::dsp::TransientDetector detector;

    for (int i = 0; i < 1000; ++i) {
        REQUIRE(!detector.process(0.0f).onset_detected);
    }

    std::vector<float> x(2000, 0.0f);
    for (int i = 0; i < 64; ++i) x[i] = (i == 0) ? 0.8f : (0.8f * std::exp(-0.05f * i));
    auto onsets = run(detector, x);
    REQUIRE(onsets.size() == 1);
    REQUIRE(onsets[0].ready);
    REQUIRE(onsets[0].final_velocity > 0.5f);
}

TEST_CASE("TransientDetector final velocity tracks attack level, not trigger instant", "[dsp][transient]") {
    float vel_soft = 0, vel_loud = 0;
    for (float amp : {0.03f, 0.6f}) {
        transducer::dsp::TransientDetector d(0.005f);
        std::vector<float> x(9600, 0.0f);
        add_pluck(x, 1000, 196.0f, amp, 0.4f);
        auto o = run(d, x);
        REQUIRE(o.size() == 1);
        REQUIRE(o[0].ready);
        (amp < 0.1f ? vel_soft : vel_loud) = o[0].final_velocity;
    }
    REQUIRE(vel_loud > 0.9f);
    REQUIRE(vel_soft < 0.5f);
    REQUIRE(vel_loud - vel_soft > 0.4f);
}

TEST_CASE("TransientDetector does not retrigger on sustained low notes", "[dsp][transient][false-trigger]") {
    transducer::dsp::TransientDetector d(0.012f);
    std::vector<float> x(96000, 0.0f);
    add_pluck(x, 500, 82.41f, 0.4f, 5.0f); // E2, very long sustain
    auto o = run(d, x);
    REQUIRE(o.size() == 1);
}

TEST_CASE("TransientDetector fires on a re-pluck over a ringing string", "[dsp][transient]") {
    transducer::dsp::TransientDetector d(0.012f);
    std::vector<float> x(48000, 0.0f);
    add_pluck(x, 500, 110.0f, 0.3f, 0.5f);
    add_pluck(x, 20000, 330.0f, 0.5f, 0.5f); // string still ringing at ~0.14
    auto o = run(d, x);
    REQUIRE(o.size() == 2);
    REQUIRE(o[1].sample >= 20000);
    REQUIRE(o[1].sample < 20000 + 480);
}

TEST_CASE("TransientDetector ignores slow swells", "[dsp][transient][false-trigger]") {
    // A swell from true silence necessarily starts with a high fast/slow ratio, so the absolute
    // floor must be above the level where the ratio has settled (sensitivity setting).
    transducer::dsp::TransientDetector d(0.05f);
    std::vector<float> x(96000);
    for (size_t i = 0; i < x.size(); ++i) {
        float g = std::min(static_cast<float>(i) / (1.5f * kSr), 1.0f); // 1.5 s swell
        x[i] = 0.4f * g * std::sin(kTwoPi * 220.0f * static_cast<float>(i) / kSr);
    }
    REQUIRE(run(d, x).empty());
}

TEST_CASE("TransientDetector triggers within 2 ms of pluck onset", "[dsp][transient][timing]") {
    transducer::dsp::TransientDetector d(0.012f);
    std::vector<float> x(4000, 0.0f);
    add_pluck(x, 1000, 220.0f, 0.4f, 0.4f);
    auto o = run(d, x);
    REQUIRE(o.size() == 1);
    REQUIRE(o[0].sample >= 1000);
    REQUIRE(o[0].sample - 1000 <= 96); // <= 2 ms @ 48 kHz
}
