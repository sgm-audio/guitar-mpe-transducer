#pragma once

#include <cstdint>
#include <cmath>
#include <algorithm>

namespace transducer::dsp {

enum class NoteActionType : uint8_t {
    kNone = 0,
    kNoteOn,
    kNoteOff,
    kBendModulate,
    kRetrigger
};

struct NoteAction {
    NoteActionType type{NoteActionType::kNone};
    int16_t key{60};
    float velocity{0.0f};
    float semitone_bend{0.0f};
    int16_t off_key{-1}; // Key to release if retriggering
};

class NoteStateMachine {
public:
    constexpr NoteStateMachine() noexcept = default;

    void reset() noexcept {
        active_ = false;
        current_key_ = 60;
        current_velocity_ = 0.0f;
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

private:
    bool active_{false};
    int16_t current_key_{60};
    float current_velocity_{0.0f};
};

} // namespace transducer::dsp
