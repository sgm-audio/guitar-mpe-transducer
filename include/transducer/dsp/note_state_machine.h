#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>

namespace transducer::dsp {

enum class NoteActionType : uint8_t {
    kNone = 0,
    kNoteOn,
    kNoteOff,
    kBendModulate,
    kRetrigger,
    kLegato // hammer-on / pull-off / slide: same string, new pitch, no new pluck
};

struct NoteAction {
    NoteActionType type{NoteActionType::kNone};
    int16_t key{60};
    float velocity{0.0f};
    float semitone_bend{0.0f};
    int16_t off_key{-1}; // Key to release if retriggering
};

// Inputs for the hop-rate `step()` API used by the real-time pipeline.
struct NoteStepInput {
    // A pending onset has just been resolved (velocity known, pitch settled).
    bool resolved_onset{false};
    float onset_midi{0.0f};
    float onset_velocity{0.0f};

    // One pitch-tracker frame this hop.
    bool pitch_frame{false};
    bool pitch_valid{false};
    float raw_midi{0.0f};       // unfiltered YIN pitch (used to detect fret jumps)
    float confidence{0.0f};
    float filtered_midi{0.0f};  // continuity-filtered pitch (used for bend)

    float level{0.0f};          // fast level follower
    float gate{0.002f};         // note-off threshold (parameter)
    bool muted{false};          // >= 30 dB drop in ~10 ms (ReleaseVelocityEstimator::muted)
};

class NoteStateMachine {
public:
    static constexpr float kOffHysteresis = 0.7f;      // off below gate*0.7, on only above gate
    static constexpr int kInvalidFramesForOff = 60;    // ~80 ms of no pitch (with low level)
    static constexpr size_t kLookbackHops = 22;        // ~30 ms @ 750 Hz hop rate
    static constexpr float kJumpSemitones = 0.85f;     // >= 0.85 st within 30 ms = fret change
    static constexpr float kJumpTolerance = 0.4f;
    static constexpr int kJumpConfirmFrames = 3;
    static constexpr int kOctaveJumpConfirmFrames = 8; // octave-like steps are usually YIN errors
    static constexpr float kMinLegatoConfidence = 0.7f;

    constexpr NoteStateMachine() noexcept = default;

    void reset() noexcept {
        active_ = false;
        current_key_ = 60;
        current_velocity_ = 0.0f;
        clear_tracking(0.0f);
    }

    [[nodiscard]] bool is_active() const noexcept { return active_; }
    [[nodiscard]] int16_t current_key() const noexcept { return current_key_; }
    [[nodiscard]] float current_velocity() const noexcept { return current_velocity_; }

    NoteAction process(bool transient_detected,
                       float raw_midi_pitch,
                       float rms_energy,
                       bool gate_active) noexcept {
        if (!active_) {
            if (gate_active && (transient_detected || rms_energy > 0.005f) && raw_midi_pitch > 0.0f) {
                active_ = true;
                current_key_ = static_cast<int16_t>(std::clamp(std::round(raw_midi_pitch), 0.0f, 127.0f));
                current_velocity_ = std::clamp(rms_energy, 0.01f, 1.0f);
                return {NoteActionType::kNoteOn, current_key_, current_velocity_, 0.0f, -1};
            }
            return {NoteActionType::kNone, 0, 0.0f, 0.0f, -1};
        }

        // Currently active note
        if (!gate_active || rms_energy < 0.001f || raw_midi_pitch <= 0.0f) {
            active_ = false;
            int16_t released_key = current_key_;
            return {NoteActionType::kNoteOff, released_key, 0.0f, 0.0f, -1};
        }

        if (transient_detected) {
            // Re-plucked: new transient while gate is held -> retrigger
            int16_t old_key = current_key_;
            current_key_ = static_cast<int16_t>(std::clamp(std::round(raw_midi_pitch), 0.0f, 127.0f));
            current_velocity_ = std::clamp(rms_energy, 0.01f, 1.0f);
            return {NoteActionType::kRetrigger, current_key_, current_velocity_, 0.0f, old_key};
        }

        // Legato pitch bend modulation
        float bend = raw_midi_pitch - static_cast<float>(current_key_);
        return {NoteActionType::kBendModulate, current_key_, current_velocity_, bend, -1};
    }

    // Hop-rate step with hysteresis, legato detection and fast mute detection.
    //  - Note off: level < gate*0.7 (hysteresis), a muted() drop, or no valid pitch for ~80 ms
    //    while the level is small.
    //  - Legato: a raw pitch step > 0.8 st that then holds for 3 frames (8 if octave-like) with
    //    no new onset -> kLegato (new key, velocity 0.8x). Smooth bends never step that fast, so
    //    they stay kBendModulate.
    NoteAction step(const NoteStepInput& in) noexcept {
        if (active_) {
            if (in.muted || in.level < in.gate * kOffHysteresis) {
                return note_off();
            }
        }

        if (in.resolved_onset && in.level >= in.gate) {
            const int16_t old_key = current_key_;
            const bool was_active = active_;
            active_ = true;
            current_key_ = to_key(in.onset_midi);
            current_velocity_ = std::clamp(in.onset_velocity, 0.01f, 1.0f);
            clear_tracking(in.onset_midi);
            return {was_active ? NoteActionType::kRetrigger : NoteActionType::kNoteOn,
                    current_key_, current_velocity_, 0.0f,
                    was_active ? old_key : static_cast<int16_t>(-1)};
        }

        if (!active_ || !in.pitch_frame) {
            return {NoteActionType::kNone, current_key_, 0.0f, 0.0f, -1};
        }

        const bool good = in.pitch_valid && in.confidence >= kMinLegatoConfidence;
        if (!in.pitch_valid) {
            if (++invalid_frames_ >= kInvalidFramesForOff && in.level < in.gate * 10.0f) {
                return note_off();
            }
            return {NoteActionType::kNone, current_key_, 0.0f, 0.0f, -1};
        }
        invalid_frames_ = 0;

        if (good) {
            // Compare against pitch ~30 ms ago (not 1 hop ago) so YIN's ~20 ms window straddle on
            // a hammer-on still registers as a discrete jump, while a 2-st bend (~0.25 st/30ms) stays bend.
            const float ref = (hist_filled_ >= kLookbackHops) ? hist_[hist_pos_] : hist_[0];
            const float step_st = (hist_filled_ > 0) ? (in.raw_midi - ref) : 0.0f;
            hist_[hist_pos_] = in.raw_midi;
            hist_pos_ = (hist_pos_ + 1) % kLookbackHops;
            if (hist_filled_ < kLookbackHops) ++hist_filled_;

            const float delta = has_prev_raw_ ? std::fabs(in.raw_midi - prev_raw_) : 0.0f;
            prev_raw_ = in.raw_midi;
            has_prev_raw_ = true;

            if (jump_frames_ == 0) {
                if (std::fabs(step_st) > kJumpSemitones) {
                    jump_frames_ = 1;
                    jump_target_ = in.raw_midi;
                    jump_octave_ = is_octave_like(step_st);
                }
            } else if (std::fabs(in.raw_midi - jump_target_) < kJumpTolerance && delta < 0.12f) {
                ++jump_frames_;
                const int need = jump_octave_ ? kOctaveJumpConfirmFrames : kJumpConfirmFrames;
                if (jump_frames_ >= need) {
                    const int16_t new_key = to_key(in.raw_midi);
                    jump_frames_ = 0;
                    if (new_key != current_key_) {
                        const int16_t old_key = current_key_;
                        current_key_ = new_key;
                        current_velocity_ = std::max(0.2f, current_velocity_ * 0.8f);
                        clear_tracking(in.raw_midi);
                        return {NoteActionType::kLegato, current_key_, current_velocity_, 0.0f, old_key};
                    }
                }
            } else if (std::fabs(in.raw_midi - ref) > kJumpSemitones) {
                // Still sliding toward the new fret: update target and restart confirmation once settled.
                jump_frames_ = 1;
                jump_target_ = in.raw_midi;
                jump_octave_ = is_octave_like(step_st);
            } else {
                jump_frames_ = 0;
            }
        }

        const float bend = in.filtered_midi - static_cast<float>(current_key_);
        return {NoteActionType::kBendModulate, current_key_, current_velocity_, bend, -1};
    }

private:
    static int16_t to_key(float midi) noexcept {
        return static_cast<int16_t>(std::clamp(std::round(midi), 0.0f, 127.0f));
    }
    static bool is_octave_like(float step) noexcept {
        const float a = std::fabs(step);
        return std::fabs(a - 12.0f) < 1.3f || std::fabs(a - 24.0f) < 1.6f;
    }
    NoteAction note_off() noexcept {
        active_ = false;
        clear_tracking(0.0f);
        return {NoteActionType::kNoteOff, current_key_, 0.0f, 0.0f, -1};
    }
    void clear_tracking(float seed_midi) noexcept {
        hist_.fill(seed_midi);
        hist_pos_ = 0;
        hist_filled_ = (seed_midi > 0.0f) ? kLookbackHops : 0;
        jump_frames_ = 0;
        jump_target_ = 0.0f;
        jump_octave_ = false;
        invalid_frames_ = 0;
        prev_raw_ = seed_midi;
        has_prev_raw_ = seed_midi > 0.0f;
    }

    std::array<float, kLookbackHops> hist_{};
    size_t hist_pos_{0};
    size_t hist_filled_{0};
    int jump_frames_{0};
    float jump_target_{0.0f};
    bool jump_octave_{false};
    int invalid_frames_{0};
    float prev_raw_{0.0f};
    bool has_prev_raw_{false};

    bool active_{false};
    int16_t current_key_{60};
    float current_velocity_{0.0f};
};

} // namespace transducer::dsp
