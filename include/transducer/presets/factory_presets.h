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

inline constexpr std::array<FactoryPreset, 25> kFactoryPresets{{
    // 1. Clean Electric (Mono): balanced pickup sensitivity, full 48-st MPE bend range
    {"Clean Electric (Mono)", "01_clean_electric_mono.xml", 48.0f, 0.0120f, 0.0020f, 0, 0},

    // 2. Light Drive (Mono): higher threshold & gate to ignore amp/pedal compression & hum
    {"Light Drive (Mono)", "02_light_drive_mono.xml", 48.0f, 0.0220f, 0.0055f, 0, 0},

    // 3. Acoustic Piezo / Mic: sensitive to fingerstyle attacks, tight gate for body resonance
    {"Acoustic Piezo / Mic", "03_acoustic.xml", 48.0f, 0.0090f, 0.0030f, 0, 0},

    // 4. Hexaphonic 6-String: per-string discrete pickup, clean low-crosstalk sensitivity
    {"Hexaphonic 6-String", "04_hexaphonic.xml", 48.0f, 0.0100f, 0.0018f, 0, 0},

    // 5. Tight Tracking: fast re-plucks, higher gate, 24-st bend range for lead synths
    {"Tight Tracking", "05_tight_tracking.xml", 24.0f, 0.0180f, 0.0065f, 0, 0},

    // 6. Single-Coil Strat / Tele: crisp attack with extra gate headroom for 60 Hz hum
    {"Single-Coil Strat / Tele", "06_single_coil_strat.xml", 48.0f, 0.0135f, 0.0035f, 0, 0},

    // 7. High-Output Humbucker: hot magnetic pickups with higher transient threshold
    {"High-Output Humbucker", "07_high_output_humbucker.xml", 48.0f, 0.0195f, 0.0040f, 0, 0},

    // 8. Active EMG / Metal DI: compressed hot signal, tight gate for palm-mutes & fast riffs
    {"Active EMG / Metal DI", "08_active_metal_di.xml", 24.0f, 0.0280f, 0.0085f, 0, 0},

    // 9. High-Gain Amp / Fuzz: heavy saturation noise floor, strong gate & high transient threshold
    {"High-Gain Amp / Fuzz", "09_high_gain_fuzz.xml", 24.0f, 0.0360f, 0.0125f, 0, 0},

    // 10. Jazz Archtop / Hollowbody: warm flatwound attack, gentle sensitivity, smooth legato
    {"Jazz Archtop / Hollowbody", "10_jazz_archtop.xml", 12.0f, 0.0085f, 0.0022f, 0, 0},

    // 11. Bass Guitar (4/5-String): strong fundamental, deeper gate to clean up string noise
    {"Bass Guitar (4/5-String)", "11_bass_guitar.xml", 12.0f, 0.0150f, 0.0045f, 0, 0},

    // 12. Fingerstyle / Classical Nylon: soft flesh/nail attacks, ultra-responsive dynamics
    {"Fingerstyle / Classical Nylon", "12_fingerstyle_nylon.xml", 48.0f, 0.0055f, 0.0012f, 0, 0},

    // 13. Slide / Bottleneck Expressive: low gate for continuous glissando without false retriggers
    {"Slide / Bottleneck Expressive", "13_slide_bottleneck.xml", 48.0f, 0.0160f, 0.0010f, 0, 0},

    // 14. E-Bow / Ambient Swell: minimal gate & gentle onset for infinite sustain & volume swells
    {"E-Bow / Ambient Swell", "14_ebow_ambient_swell.xml", 48.0f, 0.0040f, 0.0006f, 0, 0},

    // 15. Two-Hand Tapping / Legato: sensitive onset for soft hammer-ons & clean run separation
    {"Two-Hand Tapping / Legato", "15_tapping_legato.xml", 48.0f, 0.0065f, 0.0025f, 0, 0},

    // 16. Funk Percussive / Ghost Notes: high gate & snappy threshold to filter muted scratches
    {"Funk Percussive / Ghost Notes", "16_funk_percussive.xml", 12.0f, 0.0245f, 0.0095f, 0, 0},

    // 17. Wide Whammy / Divebomb (96 st): full ±96-st range for extreme tremolo & pitch pedals
    {"Wide Whammy / Divebomb (96 st)", "17_wide_whammy_96st.xml", 96.0f, 0.0140f, 0.0025f, 0, 0},

    // 18. Dual-Neck Lead (Target Highest): guitar bends modulate the top voice of latched chords
    {"Dual-Neck Lead (Target Highest)", "18_dual_neck_lead_highest.xml", 48.0f, 0.0125f, 0.0024f, 1, 0},

    // 19. Dual-Neck Chord Mod (Target All): guitar expression modulates all latched keyboard voices
    {"Dual-Neck Chord Mod (Target All)", "19_dual_neck_chord_all.xml", 48.0f, 0.0130f, 0.0028f, 2, 0},

    // 20. Dual-Neck Swell Pad (Target All): ambient swells modulating lush polyphonic synth pads
    {"Dual-Neck Swell Pad (Target All)", "20_dual_neck_swell_pad.xml", 24.0f, 0.0060f, 0.0008f, 2, 0},

    // 21. Dual-Neck Tight Lead (Highest): crisp gating for lead voice-leading over held chords
    {"Dual-Neck Tight Lead (Highest)", "21_dual_neck_tight_lead.xml", 24.0f, 0.0200f, 0.0060f, 1, 0},

    // 22. MIDI 2.0 UMP Clean Studio: 32-bit per-note pitch bend & velocity for clean electric DI
    {"MIDI 2.0 UMP Clean Studio", "22_midi2_ump_clean.xml", 48.0f, 0.0115f, 0.0018f, 0, 1},

    // 23. MIDI 2.0 UMP Hexaphonic: 6-string discrete pickup outputting full-resolution UMP
    {"MIDI 2.0 UMP Hexaphonic", "23_midi2_ump_hexaphonic.xml", 48.0f, 0.0095f, 0.0015f, 0, 1},

    // 24. MIDI 2.0 UMP Dual-Neck Poly: Target All routing over MIDI 2.0 UMP
    {"MIDI 2.0 UMP Dual-Neck Poly", "24_midi2_ump_dual_neck.xml", 48.0f, 0.0120f, 0.0022f, 2, 1},

    // 25. Classic VA Synth (±2 st): traditional ±2-st bend range for vintage/non-MPE synths
    {"Classic VA Synth (±2 st)", "25_classic_va_2st.xml", 2.0f, 0.0145f, 0.0038f, 0, 1},
}};

inline constexpr size_t kNumFactoryPresets = kFactoryPresets.size();

} // namespace transducer::presets
