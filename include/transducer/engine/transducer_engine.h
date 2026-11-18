#pragma once

#include <transducer/dsp/string_pipeline.h>
#include <transducer/voice/voice_allocator.h>
#include <transducer/params/parameter_manager.h>

#include <cstdint>

namespace transducer::engine {

struct TransducerEngineOutput {
    bool note_active{false};
    bool note_retriggered{false};
    int16_t active_key{0};
    float velocity{0.0f};
    float release_velocity{0.0f};
    float semitone_bend{0.0f};
    float brightness{0.0f};
    dsp::ArticulationType articulation{dsp::ArticulationType::kOpenPluck};
    int16_t voice_routing_target_key{-1};
    uint32_t events_emitted{0};
    uint32_t midi2_ump_count{0};
};

// Shared block-oriented engine wrapping `dsp::StringPipeline` so tests and non-CLAP wrappers
// exercise the exact same DSP pipeline as `plugin_process`.
class TransducerEngine {
public:
    TransducerEngine() = default;

    void init(float sample_rate) noexcept {
        sample_rate_ = sample_rate;
        reset();
    }

    void reset() noexcept {
        pipeline_.prepare(sample_rate_, 0);
        voice_allocator_.reset();
    }

    void set_voice_routing_mode(voice::RoutingMode mode) noexcept {
        voice_allocator_.set_routing_mode(mode);
    }

    void handle_keyboard_note_on(int32_t note_id, int16_t key, float velocity, int16_t channel = 0) noexcept {
        voice_allocator_.onKeyNoteOn(note_id, key, velocity, channel);
    }

    void handle_keyboard_note_off(int32_t note_id, int16_t key, int16_t channel = 0) noexcept {
        voice_allocator_.onKeyNoteOff(note_id, key, channel);
    }

    params::ParameterManager& params() noexcept { return params_; }
    const params::ParameterManager& params() const noexcept { return params_; }

    void process_block(const float* audio_in, size_t num_samples, TransducerEngineOutput& out) noexcept {
        if (!audio_in || num_samples == 0) return;

        out = TransducerEngineOutput{};
        const auto snap = params_.get_audio_snapshot();
        dsp::StringPipeline::Config cfg;
        cfg.transient_threshold = snap.transient_sensitivity;
        cfg.gate_threshold = snap.gate_threshold;
        cfg.chord_guard = false;
        pipeline_.set_config(cfg);

        bool saw_off = false;
        bool saw_on = false;
        for (size_t i = 0; i < num_samples; ++i) {
            pipeline_.process_sample(audio_in[i], static_cast<uint32_t>(i), [&](const dsp::CoreEvent& e) {
                ++out.events_emitted;
                using T = dsp::CoreEvent::Type;
                switch (e.type) {
                case T::kNoteOn:
                    if (saw_off) out.note_retriggered = true;
                    saw_on = true;
                    out.velocity = e.value;
                    ++out.midi2_ump_count;
                    break;
                case T::kNoteOff:
                    saw_off = true;
                    out.release_velocity = e.value;
                    ++out.midi2_ump_count;
                    break;
                case T::kBend:
                    out.semitone_bend = e.value;
                    ++out.midi2_ump_count;
                    break;
                case T::kPressure:
                    ++out.midi2_ump_count;
                    break;
                case T::kBrightness:
                    out.brightness = e.value;
                    break;
                }
            });
        }
        (void)saw_on;

        out.note_active = pipeline_.note_active();
        out.active_key = pipeline_.key();
        out.articulation = pipeline_.articulation();

        auto targets = voice_allocator_.get_modulation_targets();
        if (!targets.empty()) {
            out.voice_routing_target_key = targets[0].key;
        }
    }

private:
    float sample_rate_{48000.0f};
    dsp::StringPipeline pipeline_{};
    voice::VoiceAllocator voice_allocator_{};
    params::ParameterManager params_{};
};

} // namespace transducer::engine
