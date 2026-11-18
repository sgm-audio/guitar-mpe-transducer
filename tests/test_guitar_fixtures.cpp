#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/string_pipeline.h>
#include <cmath>
#include <random>
#include <vector>

using namespace transducer::dsp;

namespace {
constexpr float kSr = 48000.0f;
constexpr float kTwoPi = 6.28318530718f;

std::vector<CoreEvent> run_pipeline(const std::vector<float>& x, bool chord_guard = false) {
    StringPipeline p;
    p.prepare(kSr, 0);
    StringPipeline::Config cfg;
    cfg.chord_guard = chord_guard;
    p.set_config(cfg);
    std::vector<CoreEvent> ev;
    for (size_t i = 0; i < x.size(); ++i) {
        p.process_sample(x[i], static_cast<uint32_t>(i), [&](const CoreEvent& e) { ev.push_back(e); });
    }
    return ev;
}

void add_pluck(std::vector<float>& buf, size_t start, float f0, float amp, float tau) {
    for (size_t i = start; i < buf.size(); ++i) {
        float t = static_cast<float>(i - start) / kSr;
        float att = std::min(t / 0.001f, 1.0f);
        float ph = kTwoPi * f0 * t;
        buf[i] += amp * att * std::exp(-t / tau) *
                  (std::sin(ph) + 0.4f * std::sin(2 * ph) + 0.2f * std::sin(3 * ph)) / 1.6f;
    }
}
} // namespace

TEST_CASE("Guitar fixture: decaying plucks across 82 Hz .. 988 Hz", "[fixture][guitar]") {
    for (float f0 : {82.41f, 110.0f, 329.63f, 987.77f}) {
        std::vector<float> x(24000, 0.0f);
        add_pluck(x, 1000, f0, 0.4f, 0.5f);
        auto ev = run_pipeline(x);
        int ons = 0;
        int16_t key = -1;
        for (const auto& e : ev) {
            if (e.type == CoreEvent::Type::kNoteOn) { ++ons; key = e.key; }
        }
        int expected = static_cast<int>(std::round(69.0f + 12.0f * std::log2(f0 / 440.0f)));
        INFO("f0=" << f0);
        REQUIRE(ons == 1);
        REQUIRE(key == expected);
    }
}

TEST_CASE("Guitar fixture: hammer-on emits kNoteOff + kNoteOn without re-pluck", "[fixture][guitar]") {
    std::vector<float> x(36000, 0.0f);
    double ph = 0.0;
    for (size_t i = 1000; i < x.size(); ++i) {
        float f = (i < 16000) ? 220.0f : 246.94f; // A3 -> B3 (+2 st) at sample 16000
        float amp = 0.4f * std::exp(-static_cast<float>(i - 1000) / (0.8f * kSr));
        float att = std::min(static_cast<float>(i - 1000) / (0.001f * kSr), 1.0f);
        x[i] = amp * att * static_cast<float>(std::sin(ph));
        ph += kTwoPi * f / kSr;
    }
    auto ev = run_pipeline(x);
    std::vector<int16_t> keys;
    for (const auto& e : ev) if (e.type == CoreEvent::Type::kNoteOn) keys.push_back(e.key);
    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == 57);
    REQUIRE(keys[1] == 59);
}

TEST_CASE("Guitar fixture: 2-semitone bend modulates bend without retriggering", "[fixture][guitar]") {
    std::vector<float> x(36000, 0.0f);
    double ph = 0.0;
    for (size_t i = 1000; i < x.size(); ++i) {
        float frac = (i < 12000) ? 0.0f : std::min(1.0f, static_cast<float>(i - 12000) / (0.25f * kSr));
        float f = 220.0f * std::pow(2.0f, (2.0f * frac) / 12.0f);
        float att = std::min(static_cast<float>(i - 1000) / (0.001f * kSr), 1.0f);
        x[i] = 0.4f * att * static_cast<float>(std::sin(ph));
        ph += kTwoPi * f / kSr;
    }
    auto ev = run_pipeline(x);
    int ons = 0;
    float max_bend = 0.0f;
    for (const auto& e : ev) {
        if (e.type == CoreEvent::Type::kNoteOn) ++ons;
        if (e.type == CoreEvent::Type::kBend) max_bend = std::max(max_bend, e.value);
    }
    REQUIRE(ons == 1);
    REQUIRE_THAT(max_bend, Catch::Matchers::WithinAbs(2.0f, 0.25f));
}

TEST_CASE("Guitar fixture: 5.5 Hz vibrato tracks modulation and detects vibrato", "[fixture][guitar]") {
    StringPipeline p;
    p.prepare(kSr, 0);
    double ph = 0.0;
    int ons = 0;
    for (size_t i = 0; i < 36000; ++i) {
        float x = 0.0f;
        if (i >= 1000) {
            float t = static_cast<float>(i - 1000) / kSr;
            float st = 0.45f * std::sin(kTwoPi * 5.5f * t);
            float f = 220.0f * std::pow(2.0f, st / 12.0f);
            float att = std::min(t / 0.001f, 1.0f);
            x = 0.4f * att * static_cast<float>(std::sin(ph));
            ph += kTwoPi * f / kSr;
        }
        p.process_sample(x, static_cast<uint32_t>(i), [&](const CoreEvent& e) {
            if (e.type == CoreEvent::Type::kNoteOn) ++ons;
        });
    }
    REQUIRE(ons == 1);
    REQUIRE(p.vibrato_active());
}

TEST_CASE("Guitar fixture: tremolo picking triggers every pluck", "[fixture][guitar]") {
    std::vector<float> x(48000, 0.0f);
    for (int k = 0; k < 5; ++k) {
        add_pluck(x, 1000 + static_cast<size_t>(k) * 7200, 220.0f, 0.4f, 0.12f); // 150 ms apart
    }
    auto ev = run_pipeline(x);
    int ons = 0;
    for (const auto& e : ev) if (e.type == CoreEvent::Type::kNoteOn) ++ons;
    REQUIRE(ons == 5);
}

TEST_CASE("Guitar fixture: short unpitched fret noise produces zero notes", "[fixture][guitar]") {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-0.25f, 0.25f);
    std::vector<float> x(24000, 0.0f);
    for (size_t i = 1000; i < 1240; ++i) x[i] = dist(rng); // 5 ms burst of noise
    auto ev = run_pipeline(x);
    for (const auto& e : ev) REQUIRE(e.type != CoreEvent::Type::kNoteOn);
}

TEST_CASE("Guitar fixture: DC offset + 60 Hz pickup hum does not break tracking", "[fixture][guitar]") {
    std::vector<float> x(24000, 0.0f);
    add_pluck(x, 1000, 146.83f, 0.4f, 0.5f); // D3 = 50
    for (size_t i = 0; i < x.size(); ++i) {
        x[i] += 0.05f + 0.008f * std::sin(kTwoPi * 60.0f * static_cast<float>(i) / kSr);
    }
    auto ev = run_pipeline(x);
    int ons = 0;
    int16_t key = -1;
    for (const auto& e : ev) if (e.type == CoreEvent::Type::kNoteOn) { ++ons; key = e.key; }
    REQUIRE(ons == 1);
    REQUIRE(key == 50);
}

TEST_CASE("Guitar fixture: pressure stays bounded on low-level string noise", "[fixture][guitar][pressure]") {
    std::vector<float> x(24000, 0.0f);
    add_pluck(x, 1000, 220.0f, 0.08f, 0.2f);
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> n(-0.003f, 0.003f);
    for (float& s : x) s += n(rng);

    auto ev = run_pipeline(x);
    float max_p = 0.0f;
    for (const auto& e : ev) {
        if (e.type == CoreEvent::Type::kPressure) max_p = std::max(max_p, e.value);
    }
    REQUIRE(max_p < 0.35f);
}

TEST_CASE("Guitar fixture: continuous +35 cent bend maps accurately", "[fixture][guitar][cents]") {
    std::vector<float> x(24000, 0.0f);
    double ph = 0.0;
    for (size_t i = 1000; i < x.size(); ++i) {
        float frac = (i < 8000) ? 0.0f : std::min(1.0f, static_cast<float>(i - 8000) / (0.1f * kSr));
        float f = 220.0f * std::pow(2.0f, (0.35f * frac) / 12.0f);
        float att = std::min(static_cast<float>(i - 1000) / (0.001f * kSr), 1.0f);
        x[i] = 0.4f * att * static_cast<float>(std::sin(ph));
        ph += kTwoPi * f / kSr;
    }
    auto ev = run_pipeline(x);
    float last_bend = 0.0f;
    for (const auto& e : ev) if (e.type == CoreEvent::Type::kBend) last_bend = e.value;
    REQUIRE_THAT(last_bend, Catch::Matchers::WithinAbs(0.35f, 0.05f)); // within 5 cents
}
