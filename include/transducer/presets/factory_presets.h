#pragma once

#include <array>
#include <cstddef>

namespace transducer::presets {

struct FactoryPreset {
    const char* name;
    const char* filename;
    float bend_range;
    float transient_sensitivity;
    float gate_threshold;
    int routing_mode;    // 0 = Direct, 1 = Target Highest, 2 = Target All
    int output_dialect;  // 0 = CLAP / MPE, 1 = MIDI 2.0 UMP / Single-Channel
};

inline constexpr std::array<FactoryPreset, 5> kFactoryPresets{{
    // 1. Clean Electric (Mono): balanced pickup sensitivity, full 48-st MPE bend range
    {"Clean Electric (Mono)", "01_clean_electric_mono.xml", 48.0f, 0.012f, 0.0020f, 0, 0},

    // 2. Light Drive (Mono): higher threshold & gate to ignore amp/pedal compression & hum
    {"Light Drive (Mono)", "02_light_drive_mono.xml", 48.0f, 0.022f, 0.0055f, 0, 0},

    // 3. Acoustic Piezo / Mic: sensitive to fingerstyle attacks, tight gate for body resonance
    {"Acoustic Piezo / Mic", "03_acoustic.xml", 48.0f, 0.009f, 0.0030f, 0, 0},

    // 4. Hexaphonic 6-String: per-string discrete pickup, clean low-crosstalk sensitivity
    {"Hexaphonic 6-String", "04_hexaphonic.xml", 48.0f, 0.010f, 0.0018f, 0, 0},

    // 5. Tight Tracking: fast re-plucks, higher gate, 24-st bend range for lead synths
    {"Tight Tracking", "05_tight_tracking.xml", 24.0f, 0.018f, 0.0065f, 0, 0},
}};

inline constexpr size_t kNumFactoryPresets = kFactoryPresets.size();

} // namespace transducer::presets
