# guitar-mpe-transducer

> Real-time Guitar-to-CLAP / MIDI 2.0 UMP Transducer Plugin for expressive, low-latency polyphonic synthesis.

## Overview

`guitar-mpe-transducer` is a native, ultra-low-latency C++20 CLAP audio plugin designed for live dual-neck performance:
- **Audio Input:** Live electric or acoustic guitar.
- **DSP Engine:** Real-time safe, zero-heap-allocation sliding YIN pitch estimator + fast dual-envelope transient detector.
- **Outputs:**
  - **CLAP Note Expressions:** Sample-accurate `CLAP_NOTE_EXPRESSION_TUNING` (micro-tuning in semitones) and `CLAP_NOTE_EXPRESSION_PRESSURE` (string envelope dynamics).
  - **MIDI 2.0 Universal MIDI Packet (UMP):** Message Type 4 per-note pitch bend and 32-bit velocity.
- **Dual-Neck Keyboard Routing:** Route a MIDI 2.0/MPE keyboard into the plugin's note input port to latch chord voices, allowing your physical guitar plucks and bends to dynamically voice-lead and modulate software synthesizers (e.g., u-he Diva/ZebraHZ, Bitwig Studio).

## Architecture

```
[Guitar Audio In] ──► [Transient / Pluck Detector] ──► Note-On (32-bit vel)
        │
        └──► [Sliding-Window YIN (512)] ──► Continuous f0 / Cents Offset
                                                   │
                                                   ├─► CLAP Note Expressions (Tuning & Pressure)
                                                   └─► MIDI 2.0 UMP (Type 4 Per-Note Pitch Bend)
```

## Building (Headless Native CLAP)

Requirements:
- CMake 3.24+
- Modern C++20 compiler (MSVC 2022, GCC 11+, Clang 14+)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

> **Note:** The default `guitar_mpe_transducer.clap` target is headless (no `CLAP_EXT_GUI`). For a CLAP plugin with the custom GUI, build the JUCE target below.

## Building (JUCE VST3 / AU / Standalone / CLAP with UI)

To build the JUCE frontend with the JIVE GUI (including the GUI CLAP wrapper via `clap-juce-extensions`):

```bash
cmake -B build-juce -S . -DCMAKE_BUILD_TYPE=Release -DTRANSDUCER_BUILD_JUCE=ON -DTRANSDUCER_BUILD_JUCE_CLAP=ON
cmake --build build-juce --config Release --parallel
```

The GUI CLAP binary is output at `build-juce/guitar_mpe_transducer_juce_artefacts/Release/CLAP/Guitar MPE Transducer.clap` (distinct CLAP ID `com.sgm-audio.guitar-mpe-transducer.juce`).

## Factory Presets

Twenty-five factory presets are included in [`presets/`](presets/) (serialized in the versioned `GTRJ` v1 XML format) and registered as host programs in the JUCE build (`include/transducer/presets/factory_presets.h`):

| # | Preset | File | Sensitivity | Gate | Bend | Routing | Dialect | Use Case |
| :-: | :--- | :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| 01 | **Clean Electric (Mono)** | `01_clean_electric_mono.xml` | `0.0120` | `0.0020` | `±48 st` | Direct | CLAP / MPE | Standard DI electric guitar (single-coil or humbucker) |
| 02 | **Light Drive (Mono)** | `02_light_drive_mono.xml` | `0.0220` | `0.0055` | `±48 st` | Direct | CLAP / MPE | Boosted/overdriven pickups; suppresses compression noise & 60 Hz hum |
| 03 | **Acoustic Piezo / Mic** | `03_acoustic.xml` | `0.0090` | `0.0030` | `±48 st` | Direct | CLAP / MPE | Fingerstyle & flatpick acoustic; responsive to softer transients |
| 04 | **Hexaphonic 6-String** | `04_hexaphonic.xml` | `0.0100` | `0.0018` | `±48 st` | Direct | CLAP / MPE | 6-channel discrete pickup (Roland GK / Fishman / submarine); 1 pipeline per string |
| 05 | **Tight Tracking** | `05_tight_tracking.xml` | `0.0180` | `0.0065` | `±24 st` | Direct | CLAP / MPE | Fast staccato & palm-muted riffs; higher gate cuts sustain tails early |
| 06 | **Single-Coil Strat / Tele** | `06_single_coil_strat.xml` | `0.0135` | `0.0035` | `±48 st` | Direct | CLAP / MPE | Crisp single-coil attack with extra gate headroom for 60 Hz hum |
| 07 | **High-Output Humbucker** | `07_high_output_humbucker.xml` | `0.0195` | `0.0040` | `±48 st` | Direct | CLAP / MPE | Hot passive humbuckers with higher transient threshold |
| 08 | **Active EMG / Metal DI** | `08_active_metal_di.xml` | `0.0280` | `0.0085` | `±24 st` | Direct | CLAP / MPE | Compressed active pickups, tight gating for palm-mutes & fast riffs |
| 09 | **High-Gain Amp / Fuzz** | `09_high_gain_fuzz.xml` | `0.0360` | `0.0125` | `±24 st` | Direct | CLAP / MPE | Heavy saturation noise floor, strong gate & high transient threshold |
| 10 | **Jazz Archtop / Hollowbody** | `10_jazz_archtop.xml` | `0.0085` | `0.0022` | `±12 st` | Direct | CLAP / MPE | Warm flatwound attack, gentle sensitivity, smooth legato sustain |
| 11 | **Bass Guitar (4/5-String)** | `11_bass_guitar.xml` | `0.0150` | `0.0045` | `±12 st` | Direct | CLAP / MPE | Strong fundamental tracking with deeper gate for string/fret noise |
| 12 | **Fingerstyle / Classical Nylon** | `12_fingerstyle_nylon.xml` | `0.0055` | `0.0012` | `±48 st` | Direct | CLAP / MPE | Soft flesh/nail attacks and delicate dynamic expression |
| 13 | **Slide / Bottleneck Expressive** | `13_slide_bottleneck.xml` | `0.0160` | `0.0010` | `±48 st` | Direct | CLAP / MPE | Low gate for continuous glissando without premature note-off/retrigger |
| 14 | **E-Bow / Ambient Swell** | `14_ebow_ambient_swell.xml` | `0.0040` | `0.0006` | `±48 st` | Direct | CLAP / MPE | Minimal gate & gentle onset for infinite sustain & volume swells |
| 15 | **Two-Hand Tapping / Legato** | `15_tapping_legato.xml` | `0.0065` | `0.0025` | `±48 st` | Direct | CLAP / MPE | Sensitive onset for soft hammer-ons & clean rapid run separation |
| 16 | **Funk Percussive / Ghost Notes** | `16_funk_percussive.xml` | `0.0245` | `0.0095` | `±12 st` | Direct | CLAP / MPE | High gate & snappy threshold to trigger crisp stabs and ignore scratches |
| 17 | **Wide Whammy / Divebomb (96 st)** | `17_wide_whammy_96st.xml` | `0.0140` | `0.0025` | `±96 st` | Direct | CLAP / MPE | Full ±96-st bend range for Floyd Rose / Whammy extreme pitch dives |
| 18 | **Dual-Neck Lead (Target Highest)** | `18_dual_neck_lead_highest.xml` | `0.0125` | `0.0024` | `±48 st` | Highest | CLAP / MPE | Guitar bends & modulates the top voice of latched keyboard chords |
| 19 | **Dual-Neck Chord Mod (Target All)** | `19_dual_neck_chord_all.xml` | `0.0130` | `0.0028` | `±48 st` | All | CLAP / MPE | Guitar expression modulates all latched keyboard voices simultaneously |
| 20 | **Dual-Neck Swell Pad (Target All)** | `20_dual_neck_swell_pad.xml` | `0.0060` | `0.0008` | `±24 st` | All | CLAP / MPE | Ambient volume swells modulating lush polyphonic synth pads |
| 21 | **Dual-Neck Tight Lead (Highest)** | `21_dual_neck_tight_lead.xml` | `0.0200` | `0.0060` | `±24 st` | Highest | CLAP / MPE | Crisp gating for lead voice-leading over held keyboard harmony |
| 22 | **MIDI 2.0 UMP Clean Studio** | `22_midi2_ump_clean.xml` | `0.0115` | `0.0018` | `±48 st` | Direct | MIDI 2.0 UMP | 32-bit per-note pitch bend & velocity for clean electric DI |
| 23 | **MIDI 2.0 UMP Hexaphonic** | `23_midi2_ump_hexaphonic.xml` | `0.0095` | `0.0015` | `±48 st` | Direct | MIDI 2.0 UMP | 6-string discrete pickup outputting full-resolution MIDI 2.0 UMP |
| 24 | **MIDI 2.0 UMP Dual-Neck Poly** | `24_midi2_ump_dual_neck.xml` | `0.0120` | `0.0022` | `±48 st` | All | MIDI 2.0 UMP | Dual-neck Target All routing over MIDI 2.0 UMP |
| 25 | **Classic VA Synth (±2 st)** | `25_classic_va_2st.xml` | `0.0145` | `0.0038` | `±2 st` | Direct | MIDI 2.0 UMP | Traditional ±2-st pitch-wheel range for vintage/non-MPE synths |

### Loading Presets

- **JUCE (VST3 / AU / Standalone):**
  - Select any of the 25 presets directly from your DAW's built-in **Program / Preset** selector (exposed via `getNumPrograms()` / `setCurrentProgram()`), **or**
  - Import any [`presets/*.xml`](presets/) file via your host's preset loader or `setStateInformation` (`GTRJ` version `1`).
- **CLAP (`guitar_mpe_transducer.clap`):**
  - The native CLAP binary uses the compact `GTR1` binary state (`CLAP_EXT_STATE`). Set the 5 parameter values from the table above in your DAW (Bitwig / REAPER) and save as a DAW preset, or load via `clap_juce_extensions` when building with `-DTRANSDUCER_BUILD_JUCE_CLAP=ON`.

### Recommended MPE Synths & Pitch-Bend Range

Set your receiving synth's **MPE Pitch Bend Range** to match the transducer's `bend_range` parameter (both CLAP `CLAP_NOTE_EXPRESSION_TUNING` and MIDI 2.0/MPE RPN `0x00/0x00` send this automatically, but some synths require manual MPE mode activation):

- **±48 semitones (MPE standard):**
  - **Bitwig Studio (Polymer / Phase-4 / Polysynth):** Enable *MPE* on the track/device inspector; defaults to `±48 st`.
  - **Surge XT / Vital:** Enable *MPE* mode; set MPE Pitch Bend Range to `48`.
  - **u-he Diva / Zebra2 / ZebraHZ:** Enable *MPE* in the MIDI table / configuration; set Pitch Bend Range to `48`.
  - **Xfer Serum / Arturia Pigments:** Enable *MPE* in Global/Settings; verify Bend Range = `48`.
- **±24 / ±12 / ±2 semitones (Tight Tracking, Jazz, Bass, Classic VA):**
  - Best for **monophonic lead synths** or **classic VA patches** where `±48 st` feels too coarse over 14-bit MIDI pitch wheel (`±24 st` doubles pitch-wheel resolution while still covering 2-octave slides; `±2 st` matches vintage synth defaults). Set the synth's bend range to match the preset.

## Offline CSV Test Harness

Inspect `StringPipeline` tracking (`note_on`, `note_off`, `bend`, `pressure`) on recorded 1–6 channel WAVs or built-in synthetic guitar gestures without a DAW or live guitar:

```bash
# Built-in 6-channel hexaphonic chord -> CSV
./build/transducer_offline_harness --synth hex --out hex_events.csv

# Recorded mono guitar DI WAV with custom sensitivity/gate -> stdout
./build/transducer_offline_harness --wav my_guitar_di.wav --sensitivity 0.015 --gate 0.003
```

## License

MIT (c) 2026 SGM Audio
