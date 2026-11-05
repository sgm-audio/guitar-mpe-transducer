#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <transducer/dsp/brightness_estimator.h>
#include <transducer/clap/event_emitter.h>
#include <cmath>

TEST_CASE("BrightnessEstimator detects high frequency content versus pure fundamental", "[dsp][brightness]") {
    using namespace transducer::dsp;
    BrightnessEstimator estimator;
    constexpr float kSampleRate = 48000.0f;
    constexpr float kTwoPi = 6.28318530717958647692f;

    // 1. Pure low fundamental (82.4Hz warm sine wave)
    estimator.reset();
    float warm_brightness = 0.0f;
    for (int i = 0; i < 500; ++i) {
        float sample = 0.8f * std::sin(kTwoPi * 82.4f * i / kSampleRate);
        warm_brightness = estimator.process(sample, kSampleRate);
    }
    REQUIRE(warm_brightness < 0.15f);

    // 2. High harmonic rich tone (plucked near bridge with harmonics up to 2500Hz)
    estimator.reset();
    float bright_val = 0.0f;
    for (int i = 0; i < 500; ++i) {
        float sample = 0.3f * std::sin(kTwoPi * 82.4f * i / kSampleRate)
                     + 0.4f * std::sin(kTwoPi * 1800.0f * i / kSampleRate)
                     + 0.5f * std::sin(kTwoPi * 3200.0f * i / kSampleRate);
        bright_val = estimator.process(sample, kSampleRate);
    }
    REQUIRE(bright_val > 0.5f);
}

TEST_CASE("EventEmitter emits CLAP_NOTE_EXPRESSION_BRIGHTNESS", "[clap][brightness]") {
    using namespace transducer::clap;
    struct TestOutEvents {
        clap_output_events_t list{};
        clap_event_note_expression_t stored{};
        bool pushed{false};

        TestOutEvents() {
            list.ctx = this;
            list.try_push = [](const clap_output_events_t* out, const clap_event_header_t* hdr) -> bool {
                auto* self = static_cast<TestOutEvents*>(out->ctx);
                if (hdr->type == CLAP_EVENT_NOTE_EXPRESSION) {
                    self->stored = *reinterpret_cast<const clap_event_note_expression_t*>(hdr);
                    self->pushed = true;
                }
                return true;
            };
        }
    } mock;

    REQUIRE(EventEmitter::emitBrightnessExpression(&mock.list, 0, 100, 60, 0.82));
    REQUIRE(mock.pushed);
    REQUIRE(mock.stored.expression_id == CLAP_NOTE_EXPRESSION_BRIGHTNESS);
    REQUIRE_THAT(mock.stored.value, Catch::Matchers::WithinAbs(0.82, 1e-4));
}
