#pragma once

#include <array>
#include <cstdint>
#include <vector>
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
    }

    void set_routing_mode(RoutingMode mode) {
        mode_ = mode;
    }

    RoutingMode routing_mode() const { return mode_; }

    void onKeyNoteOn(int32_t note_id, int16_t key, float velocity, int16_t channel = 0) {
        // Find existing or free slot
        int free_idx = -1;
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].is_active && voices_[i].key == key && voices_[i].channel == channel) {
                // Re-triggering existing key
                voices_[i].note_id = note_id;
                voices_[i].velocity = velocity;
                return;
            }
            if (!voices_[i].is_active && free_idx == -1) {
                free_idx = static_cast<int>(i);
            }
        }

        if (free_idx != -1) {
            voices_[free_idx].note_id = note_id;
            voices_[free_idx].key = key;
            voices_[free_idx].channel = channel;
            voices_[free_idx].velocity = velocity;
            voices_[free_idx].is_active = true;
            ++active_count_;
        }
    }

    void onKeyNoteOff(int32_t note_id, int16_t key, int16_t channel = 0) {
        for (size_t i = 0; i < kMaxVoices; ++i) {
            if (voices_[i].is_active &&
                (voices_[i].note_id == note_id || (voices_[i].key == key && voices_[i].channel == channel)))
            {
                voices_[i].is_active = false;
                if (active_count_ > 0) {
                    --active_count_;
                }
                break;
            }
        }
    }

    size_t active_voice_count() const {
        return active_count_;
    }

    std::vector<Voice> get_modulation_targets() const {
        std::vector<Voice> targets;
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
    RoutingMode mode_{RoutingMode::kDirect};
};

} // namespace transducer::voice
