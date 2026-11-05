#pragma once

#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace transducer::midi2 {

class UmpBuilder {
public:
    // Message Type 4 (MIDI 2.0 Channel Voice Message)
    static constexpr uint8_t kTypeChannelVoice2 = 0x4;
    static constexpr uint8_t kStatusNoteOff = 0x8;
    static constexpr uint8_t kStatusNoteOn = 0x9;
    static constexpr uint8_t kStatusPolyPressure = 0xA;
    static constexpr uint8_t kStatusPerNotePitchBend = 0x6;

    static inline std::array<uint32_t, 2> makeNoteOn(
        uint8_t group, uint8_t channel, uint8_t note, float velocity)
    {
        uint32_t w0 = (static_cast<uint32_t>(kTypeChannelVoice2) << 28)
                    | (static_cast<uint32_t>(group & 0xFu) << 24)
                    | (static_cast<uint32_t>(kStatusNoteOn) << 20)
                    | (static_cast<uint32_t>(channel & 0xFu) << 16)
                    | (static_cast<uint32_t>(note & 0x7Fu) << 8);

        uint16_t vel16 = static_cast<uint16_t>(
            std::clamp(velocity, 0.0f, 1.0f) * 65535.0f);
        uint32_t w1 = static_cast<uint32_t>(vel16) << 16;
        return {w0, w1};
    }

    static inline std::array<uint32_t, 2> makeNoteOff(
        uint8_t group, uint8_t channel, uint8_t note, float velocity = 0.0f)
    {
        uint32_t w0 = (static_cast<uint32_t>(kTypeChannelVoice2) << 28)
                    | (static_cast<uint32_t>(group & 0xFu) << 24)
                    | (static_cast<uint32_t>(kStatusNoteOff) << 20)
                    | (static_cast<uint32_t>(channel & 0xFu) << 16)
                    | (static_cast<uint32_t>(note & 0x7Fu) << 8);

        uint16_t vel16 = static_cast<uint16_t>(
            std::clamp(velocity, 0.0f, 1.0f) * 65535.0f);
        uint32_t w1 = static_cast<uint32_t>(vel16) << 16;
        return {w0, w1};
    }

    static inline std::array<uint32_t, 2> makePerNotePitchBend(
        uint8_t group, uint8_t channel, uint8_t note, float semitones, float pitch_bend_range = 48.0f)
    {
        uint32_t w0 = (static_cast<uint32_t>(kTypeChannelVoice2) << 28)
                    | (static_cast<uint32_t>(group & 0xFu) << 24)
                    | (static_cast<uint32_t>(kStatusPerNotePitchBend) << 20)
                    | (static_cast<uint32_t>(channel & 0xFu) << 16)
                    | (static_cast<uint32_t>(note & 0x7Fu) << 8);

        uint32_t data32 = 0x80000000u; // Center
        if (semitones >= pitch_bend_range) {
            data32 = 0xFFFFFFFFu;
        } else if (semitones <= -pitch_bend_range) {
            data32 = 0x00000000u;
        } else if (semitones == 0.0f) {
            data32 = 0x80000000u;
        } else {
            double norm = (static_cast<double>(semitones) / static_cast<double>(pitch_bend_range)) * 0.5 + 0.5;
            norm = std::clamp(norm, 0.0, 1.0);
            data32 = static_cast<uint32_t>(norm * 4294967295.0);
        }

        return {w0, data32};
    }

    static inline std::array<uint32_t, 2> makePolyPressure(
        uint8_t group, uint8_t channel, uint8_t note, float pressure)
    {
        uint32_t w0 = (static_cast<uint32_t>(kTypeChannelVoice2) << 28)
                    | (static_cast<uint32_t>(group & 0xFu) << 24)
                    | (static_cast<uint32_t>(kStatusPolyPressure) << 20)
                    | (static_cast<uint32_t>(channel & 0xFu) << 16)
                    | (static_cast<uint32_t>(note & 0x7Fu) << 8);

        double norm = std::clamp(static_cast<double>(pressure), 0.0, 1.0);
        uint32_t data32 = static_cast<uint32_t>(norm * 4294967295.0);
        return {w0, data32};
    }
};

} // namespace transducer::midi2
