#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <transducer/engine/transducer_engine.h>
#include <vector>
#include <cmath>

namespace {

constexpr double kPi = 3.14159265358979323846;

std::vector<float> generate_continuous_tone(float freq_hz, float sample_rate, size_t num_samples, double& phase, float amplitude = 0.5f) {
    std::vector<float> audio(num_samples);
    double phase_inc = 2.0 * kPi * static_cast<double>(freq_hz) / static_cast<double>(sample_rate);
    for (size_t i = 0; i < num_samples; ++i) {
        audio[i] = amplitude * static_cast<float>(std::sin(phase));
        phase = std::fmod(phase + phase_inc, 2.0 * kPi);
    }
    return audio;
}

} // namespace

TEST_CASE("V10 Integrated Transducer Engine - Full Performance Pipeline", "[engine][v10]") {
    using namespace transducer::engine;
    using namespace transducer::dsp;

    TransducerEngine engine;
    engine.init(48000.0f);

    SECTION("1. Initial open pluck triggers NoteOn with CLAP and MIDI 2.0 UMP") {
        double phase = 0.0;
        auto tone = generate_continuous_tone(329.63f, 48000.0f, 1024, phase, 0.7f); // E4 (MIDI 64)
        
        TransducerEngineOutput out{};
        engine.process_block(tone.data(), tone.size(), out);

        REQUIRE(out.note_active);
        REQUIRE(out.active_key == 64);
        REQUIRE(out.velocity > 0.1f);
        REQUIRE(out.articulation == ArticulationType::kOpenPluck);
        REQUIRE(out.events_emitted > 0);
        REQUIRE(out.midi2_ump_count > 0);
    }

    SECTION("2. Sustained legato bend modulates pitch without re-triggering NoteOn") {
        double phase = 0.0;
        // First block: initial attack at E4 (64.0)
        auto tone1 = generate_continuous_tone(329.63f, 48000.0f, 1024, phase, 0.5f);
        TransducerEngineOutput out1{};
        engine.process_block(tone1.data(), tone1.size(), out1);
        REQUIRE(out1.note_active);
        REQUIRE(out1.active_key == 64);
        REQUIRE(out1.note_retriggered == false);

        // Second block: bend up 2 semitones to F#4 (370.0 Hz) smoothly without pluck attack
        auto tone2 = generate_continuous_tone(369.99f, 48000.0f, 1024, phase, 0.45f);
        TransducerEngineOutput out2{};
        engine.process_block(tone2.data(), tone2.size(), out2);

        REQUIRE(out2.note_active);
        REQUIRE(out2.active_key == 64); // Fundamental anchor remains key 64
        REQUIRE(out2.semitone_bend == Catch::Approx(2.0f).margin(0.35f));
        REQUIRE(out2.note_retriggered == false); // Legato modulation!
    }

    SECTION("3. Dual-neck routing routes guitar expression to keyboard chord voice") {
        engine.set_voice_routing_mode(transducer::voice::RoutingMode::kTargetHighest);

        // Simulate keyboard input with 3 notes: C4 (60), E4 (64), G4 (67)
        engine.handle_keyboard_note_on(1, 60, 0.8f);
        engine.handle_keyboard_note_on(2, 64, 0.8f);
        engine.handle_keyboard_note_on(3, 67, 0.8f);

        double phase = 0.0;
        auto tone = generate_continuous_tone(392.00f, 48000.0f, 1024, phase, 0.6f); // G4
        TransducerEngineOutput out{};
        engine.process_block(tone.data(), tone.size(), out);

        REQUIRE(out.voice_routing_target_key == 67); // Highest note targeted!
    }

    SECTION("4. Choke / palm mute triggers NoteOff with high release velocity") {
        // Pluck note
        double phase = 0.0;
        auto tone = generate_continuous_tone(220.0f, 48000.0f, 1024, phase, 0.7f);
        TransducerEngineOutput out1{};
        engine.process_block(tone.data(), tone.size(), out1);
        REQUIRE(out1.note_active);

        // Instant choke (silence with abrupt palm mute transient)
        std::vector<float> silence(1024, 0.0f);
        TransducerEngineOutput out2{};
        engine.process_block(silence.data(), silence.size(), out2);

        REQUIRE_FALSE(out2.note_active);
        REQUIRE(out2.release_velocity > 0.0f);
    }
}
