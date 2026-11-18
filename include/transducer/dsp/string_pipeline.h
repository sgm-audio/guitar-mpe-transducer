#pragma once

// One monophonic string's complete analysis chain, independent of CLAP/JUCE/MIDI:
//   audio -> onset + level -> YIN (post-onset window) -> resolver -> note state machine
//         -> CoreEvents (note on/off, bend, pressure, brightness), throttled.
// Wrappers instantiate one pipeline per string (hexaphonic) or one for a mono pickup, feed
// samples, and translate CoreEvents to their own event types. No allocation, locks or I/O.

#include <transducer/dsp/articulation_classifier.h>
#include <transducer/dsp/brightness_estimator.h>
#include <transducer/dsp/chord_guard.h>
#include <transducer/dsp/note_state_machine.h>
#include <transducer/dsp/onset_resolver.h>
#include <transducer/dsp/pitch_continuity.h>
#include <transducer/dsp/release_velocity_estimator.h>
#include <transducer/dsp/transient_detector.h>
#include <transducer/dsp/vibrato_detector.h>
#include <transducer/dsp/yin_pitch_tracker.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace transducer::dsp {

struct CoreEvent {
    enum class Type : uint8_t { kNoteOn, kNoteOff, kBend, kPressure, kBrightness };

    Type type{Type::kNoteOn};
    uint32_t offset{0};        // sample offset within the current block
    int32_t note_id{-1};
    int16_t key{0};
    int16_t string_index{0};
    // kNoteOn: velocity 0..1 | kNoteOff: release velocity 0..1 | kBend: semitones relative to key
    // kPressure / kBrightness: 0..1
    float value{0.0f};
};

class StringPipeline {
public:
    struct Config {
        float transient_threshold{0.012f};
        float gate_threshold{0.002f};
        bool chord_guard{false}; // mono pickup: suppress notes while a strummed chord is detected
    };

    // Expression throttling: at most 250 Hz per note, and only when a value moved noticeably.
    static constexpr float kExpressionIntervalSeconds = 0.004f;
    static constexpr float kBendEpsilon = 0.02f;       // semitones
    static constexpr float kPressureEpsilon = 0.01f;
    static constexpr float kBrightnessEpsilon = 0.02f;

    void prepare(float sample_rate, int string_index) noexcept {
        sr_ = sample_rate;
        string_index_ = static_cast<int16_t>(string_index);
        next_id_ = (string_index + 1) * 100000;
        transient_.set_sample_rate(sample_rate);
        resolver_.prepare(sample_rate);
        const float hop_hz = sample_rate / static_cast<float>(YinPitchTracker::kHopSize);
        vibrato_.prepare(hop_hz);
        release_.prepare(hop_hz);
        articulation_.prepare(sample_rate);
        chord_.prepare(sample_rate);
        lvl_att_ = 1.0f - std::exp(-1.0f / (0.001f * sample_rate));
        lvl_rel_ = 1.0f - std::exp(-1.0f / (0.003f * sample_rate));
        expression_gap_ = static_cast<uint64_t>(kExpressionIntervalSeconds * sample_rate);
        reset();
    }

    void reset() noexcept {
        transient_.reset();
        tracker_.reset();
        continuity_.reset();
        resolver_.reset();
        brightness_.reset();
        vibrato_.reset();
        release_.reset();
        sm_.reset();
        chord_.reset();
        articulation_.reset();
        level_ = 0.0f;
        clock_ = 0;
        note_id_ = -1;
        fresh_ = true;
        last_articulation_ = ArticulationType::kOpenPluck;
        has_f0_ = false;
    }

    void set_config(const Config& c) noexcept {
        cfg_ = c;
        transient_.set_threshold(c.transient_threshold);
    }

    bool note_active() const noexcept { return sm_.is_active(); }
    int32_t note_id() const noexcept { return note_id_; }
    int16_t key() const noexcept { return sm_.current_key(); }
    float level() const noexcept { return level_; }
    bool vibrato_active() const noexcept { return vibrato_.is_active(); }
    ArticulationType articulation() const noexcept { return last_articulation_; }

    // Feed one sample. `offset` is the sample's position in the host block; `sink(const
    // CoreEvent&)` is called for every event produced (possibly several per sample).
    template <class Sink>
    void process_sample(float x, uint32_t offset, Sink&& sink) noexcept {
        ++clock_;
        const TransientResult tr = transient_.process(x);

        const float ax = std::fabs(x);
        level_ += (ax > level_ ? lvl_att_ : lvl_rel_) * (ax - level_);
        if (level_ < 1e-20f) level_ = 0.0f;

        if (tr.onset_detected) on_onset(offset, sink);
        if (tr.velocity_ready) resolver_.on_velocity(tr.final_velocity);
        resolver_.tick();

        articulation_.feed(x);
        brightness_value_ = brightness_.process(x, sr_);

        const auto pitch = tracker_.processSample(x, sr_);
        if (pitch.has_value()) on_hop(*pitch, offset, sink);
    }

private:
    template <class Sink>
    void on_onset(uint32_t offset, Sink& sink) noexcept {
        if (cfg_.chord_guard && chord_.on_onset(clock_)) {
            // Strummed chord on a mono pickup: a single pitch would be wrong. Drop everything.
            resolver_.reset();
            if (sm_.is_active()) {
                emit(sink, CoreEvent::Type::kNoteOff, offset, sm_.current_key(), release_.release_velocity());
                sm_.reset();
                note_id_ = -1;
            }
            return;
        }
        articulation_.reset();
        has_f0_ = false;
        tracker_.notify_onset();   // analyse post-onset audio only
        continuity_.reset();       // first frame of the new note is taken raw
        resolver_.on_onset();
    }

    template <class Sink>
    void on_hop(const PitchEstimate& p, uint32_t offset, Sink& sink) noexcept {
        const float filtered = continuity_.process(p.midi_note, p.confidence, p.is_valid);
        release_.update(level_);

        if (level_ < cfg_.gate_threshold * NoteStateMachine::kOffHysteresis) {
            resolver_.reset(); // nothing audible to resolve
        }

        ResolvedNote rn{};
        const bool resolved = resolver_.on_pitch(p.is_valid, filtered, p.confidence, rn);

        NoteStepInput in;
        in.resolved_onset = resolved;
        in.onset_midi = rn.midi;
        in.onset_velocity = rn.velocity;
        in.pitch_frame = true;
        in.pitch_valid = p.is_valid && !resolver_.pending();
        in.raw_midi = p.midi_note;
        in.confidence = p.confidence;
        in.filtered_midi = filtered;
        in.level = level_;
        in.gate = cfg_.gate_threshold;
        in.muted = release_.muted();

        if (p.is_valid) { f0_ = p.f0_hz; has_f0_ = true; }
        if (articulation_.ready() && has_f0_) {
            last_articulation_ = articulation_.classify(f0_);
        } else if (articulation_.ready()) {
            last_articulation_ = articulation_.classify(0.0f);
        }

        const NoteAction act = sm_.step(in);
        switch (act.type) {
        case NoteActionType::kNoteOn:
            start_note(sink, offset, act.key, act.velocity);
            break;
        case NoteActionType::kRetrigger:
        case NoteActionType::kLegato:
            emit(sink, CoreEvent::Type::kNoteOff, offset, act.off_key, 0.0f);
            if (act.type == NoteActionType::kLegato) continuity_.reset();
            start_note(sink, offset, act.key, act.velocity);
            break;
        case NoteActionType::kNoteOff:
            emit(sink, CoreEvent::Type::kNoteOff, offset, act.key, release_.release_velocity());
            note_id_ = -1;
            resolver_.reset();
            break;
        case NoteActionType::kBendModulate:
            if (!resolver_.pending()) {
                vibrato_.process(filtered);
                emit_expression(sink, offset, act);
            }
            break;
        case NoteActionType::kNone:
        default:
            break;
        }
    }

    template <class Sink>
    void start_note(Sink& sink, uint32_t offset, int16_t key, float velocity) noexcept {
        note_id_ = ++next_id_;
        fresh_ = true;
        emit(sink, CoreEvent::Type::kNoteOn, offset, key, velocity);
    }

    template <class Sink>
    void emit_expression(Sink& sink, uint32_t offset, const NoteAction& act) noexcept {
        if (note_id_ == -1) return;
        if (!fresh_ && (clock_ - last_expression_clock_) < expression_gap_) return;

        const float pressure = std::clamp(level_ * 6.0f, 0.0f, 1.0f);
        const float brightness = brightness_value_;
        bool any = false;

        if (fresh_ || std::fabs(act.semitone_bend - last_bend_) >= kBendEpsilon) {
            emit(sink, CoreEvent::Type::kBend, offset, act.key, act.semitone_bend);
            last_bend_ = act.semitone_bend;
            any = true;
        }
        if (fresh_ || std::fabs(pressure - last_pressure_) >= kPressureEpsilon) {
            emit(sink, CoreEvent::Type::kPressure, offset, act.key, pressure);
            last_pressure_ = pressure;
            any = true;
        }
        if (fresh_ || std::fabs(brightness - last_brightness_) >= kBrightnessEpsilon) {
            emit(sink, CoreEvent::Type::kBrightness, offset, act.key, brightness);
            last_brightness_ = brightness;
            any = true;
        }
        if (any) last_expression_clock_ = clock_;
        fresh_ = false;
    }

    template <class Sink>
    void emit(Sink& sink, CoreEvent::Type type, uint32_t offset, int16_t key, float value) noexcept {
        CoreEvent e;
        e.type = type;
        e.offset = offset;
        e.note_id = note_id_;
        e.key = key;
        e.string_index = string_index_;
        e.value = value;
        sink(e);
    }

    Config cfg_{};
    float sr_{48000.0f};
    int16_t string_index_{0};
    int32_t next_id_{100000};
    int32_t note_id_{-1};

    TransientDetector transient_{};
    YinPitchTracker tracker_{};
    PitchContinuityFilter continuity_{};
    OnsetResolver resolver_{};
    NoteStateMachine sm_{};
    BrightnessEstimator brightness_{};
    VibratoDetector vibrato_{};
    ReleaseVelocityEstimator release_{};
    ArticulationClassifier articulation_{};
    ChordGuard chord_{};

    float lvl_att_{0.0f};
    float lvl_rel_{0.0f};
    float level_{0.0f};
    float brightness_value_{0.1f};
    uint64_t clock_{0};

    uint64_t expression_gap_{192};
    uint64_t last_expression_clock_{0};
    bool fresh_{true};
    float last_bend_{0.0f};
    float last_pressure_{0.0f};
    float last_brightness_{0.0f};

    float f0_{0.0f};
    bool has_f0_{false};
    ArticulationType last_articulation_{ArticulationType::kOpenPluck};
};

} // namespace transducer::dsp
