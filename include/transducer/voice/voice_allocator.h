#pragma once

#include <array>
#include <cstdint>
#include <cstddef>
#include <algorithm>

namespace transducer::voice {

enum class RoutingMode : uint8_t {
    kDirect = 0,         // Guitar emits its own voices directly
    kTargetHighest = 1,   // Modulates highest held keyboard note
    kTargetAll = 2        // Modulates all currently held keyboard notes
};

struct Voice {
    int32_t note_id{-1};
    int16_t key{-1};
    int16_t channel{0};
    float velocity{0.0f};
    bool is_active{false};
    uint32_t age{0}; // allocation order, used to steal the oldest voice when full
};

// Fixed-capacity result of a routing query (no heap allocation; safe on the audio thread).
struct ModulationTargets {
    std::array<Voice, 16> voices{};
    size_t count{0};

    size_t size() const { return count; }
    bool empty() const { return count == 0; }
    const Voice& operator[](size_t i) const { return voices[i]; }
    const Voice* begin() const { return voices.data(); }
    const Voice* end() const { return voices.data() + count; }
    void push_back(const Voice& v) { if (count < voices.size()) voices[count++] = v; }
};

class VoiceAllocator {
public:
    static constexpr size_t kMaxVoices = 16;

    VoiceAllocator() {
        for (auto& v : voices_) {
            v.is_active = false;
        }
    }

    void reset() {
        for (auto& v : voices_) {
            v.is_active = false;
        }
        active_count_ = 0;
        age_counter_ = 0;
    }

    void set_routing_mode(RoutingMode mode) {
        mode_ = mode;
    }

    RoutingMode routing_mode() const { return mode_; }

    void onKeyNoteOn(int32_t note_id, int16_t key, float velocity, int16_t channel = 0) {
        int free_idx = -1;
        int oldest_idx = 0;
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].is_active && voices_[i].key == key && voices_[i].channel == channel) {
                // Re-triggering existing key
                voices_[i].note_id = note_id;
                voices_[i].velocity = velocity;
                voices_[i].age = ++age_counter_;
                return;
            }
            if (!voices_[i].is_active && free_idx == -1) {
                free_idx = static_cast<int>(i);
            }
            if (voices_[i].age < voices_[static_cast<size_t>(oldest_idx)].age) {
                oldest_idx = static_cast<int>(i);
            }
        }

        if (free_idx == -1) {
            // All slots in use: steal the oldest voice rather than dropping the new note.
            free_idx = oldest_idx;
            voices_[static_cast<size_t>(free_idx)].is_active = false;
            if (active_count_ > 0) --active_count_;
        }

        Voice& v = voices_[static_cast<size_t>(free_idx)];
        v.note_id = note_id;
        v.key = key;
        v.channel = channel;
        v.velocity = velocity;
        v.is_active = true;
        v.age = ++age_counter_;
        ++active_count_;
    }

    // CLAP semantics: -1 for note_id / key / channel is a wildcard. A voice that has no host
    // note id (-1) matches on key/channel only. Every matching voice is released.
    void onKeyNoteOff(int32_t note_id, int16_t key, int16_t channel = 0) {
        for (size_t i = 0; i < kMaxVoices; ++i) {
            Voice& v = voices_[i];
            if (!v.is_active) continue;
            const bool id_ok = note_id == -1 || v.note_id == -1 || v.note_id == note_id;
            const bool key_ok = key == -1 || v.key == key;
            const bool ch_ok = channel == -1 || v.channel == channel;
            if (id_ok && key_ok && ch_ok) {
                v.is_active = false;
                if (active_count_ > 0) --active_count_;
            }
        }
    }

    size_t active_voice_count() const {
        return active_count_;
    }

    ModulationTargets get_modulation_targets() const {
        ModulationTargets targets;
        if (active_count_ == 0 || mode_ == RoutingMode::kDirect) {
            return targets;
        }

        if (mode_ == RoutingMode::kTargetAll) {
            for (size_t i = 0; i < kMaxVoices; ++i) {
                if (voices_[i].is_active) {
                    targets.push_back(voices_[i]);
                }
            }
            return targets;
        }

        if (mode_ == RoutingMode::kTargetHighest) {
            int highest_idx = -1;
            int16_t highest_key = -1;
            for (size_t i = 0; i < kMaxVoices; ++i) {
                if (voices_[i].is_active && voices_[i].key > highest_key) {
                    highest_key = voices_[i].key;
                    highest_idx = static_cast<int>(i);
                }
            }
            if (highest_idx != -1) {
                targets.push_back(voices_[highest_idx]);
            }
        }

        return targets;
    }

private:
    std::array<Voice, kMaxVoices> voices_{};
    size_t active_count_{0};
    uint32_t age_counter_{0};
    RoutingMode mode_{RoutingMode::kDirect};
};

} // namespace transducer::voice
