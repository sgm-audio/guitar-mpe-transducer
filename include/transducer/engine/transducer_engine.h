#pragma once

#include <transducer/dsp/transient_detector.h>
#include <transducer/dsp/yin_pitch_tracker.h>
#include <transducer/dsp/pitch_continuity.h>
#include <transducer/dsp/brightness_estimator.h>
#include <transducer/dsp/articulation_classifier.h>
#include <transducer/dsp/vibrato_detector.h>
#include <transducer/dsp/release_velocity_estimator.h>
#include <transducer/dsp/note_state_machine.h>
#include <transducer/voice/voice_allocator.h>
#include <transducer/midi2/ump_builder.h>
#include <transducer/params/parameter_manager.h>

#include <cstdint>
#include <cmath>
#include <algorithm>

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

class TransducerEngine {
public:
    TransducerEngine() = default;

    void init(float sample_rate) noexcept {
        sample_rate_ = sample_rate;
        reset();
    }

    void reset() noexcept {
        transient_detector_.reset();
        pitch_tracker_.reset();
        pitch_continuity_.reset();
        brightness_estimator_.reset();
        articulation_classifier_.reset();
        vibrato_detector_.reset();
        release_estimator_.reset();
        voice_allocator_.reset();
        state_machine_.reset();
        note_id_counter_ = 1000;
        current_note_id_ = -1;
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
        auto snap = params_.get_audio_snapshot();
        transient_detector_.set_threshold(snap.transient_sensitivity);

        bool any_transient = false;
        float current_envelope = 0.0f;
        float max_transient_vel = 0.0f;
        float rms_sum = 0.0f;

        for (size_t i = 0; i < num_samples; ++i) {
            float s = audio_in[i];
            rms_sum += s * s;
            auto res = transient_detector_.process(s);
            if (res.onset_detected) {
                any_transient = true;
                if (res.velocity > max_transient_vel) max_transient_vel = res.velocity;
            }
            current_envelope = res.envelope;

            float b = brightness_estimator_.process(s, sample_rate_);
            articulation_classifier_.feedSample(res.envelope, b);
            pitch_tracker_.processSample(s, sample_rate_);
        }

        float rms = std::sqrt(rms_sum / static_cast<float>(num_samples));
        float brightness = brightness_estimator_.brightness();
        out.brightness = brightness;

        auto raw_pitch = pitch_tracker_.latest_estimate();
        float filtered_pitch = pitch_continuity_.process(raw_pitch.midi_note, raw_pitch.confidence, raw_pitch.is_valid);

        release_estimator_.process(current_envelope);

        bool gate_active = (rms > snap.gate_threshold);

        float synth_vel = any_transient ? std::max(max_transient_vel, rms * 2.0f) : std::clamp(rms * 2.0f, 0.1f, 1.0f);

        auto action = state_machine_.process(
            any_transient,
            filtered_pitch,
            synth_vel,
            gate_active
        );

        out.articulation = articulation_classifier_.classify();

        switch (action.type) {
        case dsp::NoteActionType::kNoteOn: {
            current_note_id_ = note_id_counter_++;
            out.note_active = true;
            out.note_retriggered = false;
            out.active_key = action.key;
            out.velocity = action.velocity;
            out.events_emitted++; // Note On event
            out.midi2_ump_count++; // MIDI 2.0 Note On UMP
            break;
        }
        case dsp::NoteActionType::kRetrigger: {
            current_note_id_ = note_id_counter_++;
            out.note_active = true;
            out.note_retriggered = true;
            out.active_key = action.key;
            out.velocity = action.velocity;
            out.events_emitted += 2; // Note Off old + Note On new
            out.midi2_ump_count += 2;
            break;
        }
        case dsp::NoteActionType::kBendModulate: {
            out.note_active = true;
            out.note_retriggered = false;
            out.active_key = action.key;
            out.semitone_bend = action.semitone_bend;
            vibrato_detector_.process(filtered_pitch);
            out.events_emitted += 3; // Tuning, Pressure, Brightness
            out.midi2_ump_count++;  // Type 4 Per-Note Pitch Bend UMP
            break;
        }
        case dsp::NoteActionType::kNoteOff: {
            out.note_active = false;
            out.active_key = action.key;
            out.release_velocity = release_estimator_.release_velocity();
            out.events_emitted++; // Note Off
            out.midi2_ump_count++; // MIDI 2.0 Note Off UMP
            current_note_id_ = -1;
            break;
        }
        case dsp::NoteActionType::kNone:
        default:
            out.note_active = state_machine_.is_active();
            out.active_key = state_machine_.current_key();
            break;
        }

        // Voice allocator dual-neck routing
        auto targets = voice_allocator_.get_modulation_targets();
        if (!targets.empty()) {
            out.voice_routing_target_key = targets[0].key;
        }
    }

private:
    float sample_rate_{48000.0f};
    dsp::TransientDetector transient_detector_{};
    dsp::YinPitchTracker pitch_tracker_{};
    dsp::PitchContinuityFilter pitch_continuity_{};
    dsp::BrightnessEstimator brightness_estimator_{};
    dsp::ArticulationClassifier articulation_classifier_{};
    dsp::VibratoDetector vibrato_detector_{};
    dsp::ReleaseVelocityEstimator release_estimator_{};
    dsp::NoteStateMachine state_machine_{};
    voice::VoiceAllocator voice_allocator_{};
    params::ParameterManager params_{};

    int32_t note_id_counter_{1000};
    int32_t current_note_id_{-1};
};

} // namespace transducer::engine
