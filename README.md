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

Five factory presets are included in [`presets/`](presets/) (serialized in the versioned `GTRJ` v1 XML format) and registered as host programs in the JUCE build (`include/transducer/presets/factory_presets.h`):

| Preset | File | Sensitivity | Gate | Bend Range | Use Case |
| :--- | :--- | :---: | :---: | :---: | :--- |
| **Clean Electric (Mono)** | `01_clean_electric_mono.xml` | `0.0120` | `0.0020` | `±48 st` | Standard DI electric guitar (single-coil or humbucker) |
| **Light Drive (Mono)** | `02_light_drive_mono.xml` | `0.0220` | `0.0055` | `±48 st` | Boosted/overdriven pickups; suppresses compression noise & 60 Hz hum |
| **Acoustic Piezo / Mic** | `03_acoustic.xml` | `0.0090` | `0.0030` | `±48 st` | Fingerstyle & flatpick acoustic; responsive to softer transients |
| **Hexaphonic 6-String** | `04_hexaphonic.xml` | `0.0100` | `0.0018` | `±48 st` | 6-channel discrete pickup (Roland GK / Fishman / submarine); 1 pipeline per string |
| **Tight Tracking** | `05_tight_tracking.xml` | `0.0180` | `0.0065` | `±24 st` | Fast staccato & palm-muted riffs; higher gate cuts sustain tails early |

### Loading Presets

- **JUCE (VST3 / AU / Standalone):**
  - Select any of the 5 presets directly from your DAW's built-in **Program / Preset** selector (exposed via `getNumPrograms()` / `setCurrentProgram()`), **or**
  - Import any [`presets/*.xml`](presets/) file via your host's preset loader or `setStateInformation` (`GTRJ` version `1`).
- **CLAP (`guitar_mpe_transducer.clap`):**
  - The native CLAP binary uses the compact `GTR1` binary state (`CLAP_EXT_STATE`). Set the 5 parameter values from the table above in your DAW (Bitwig / REAPER) and save as a DAW preset, or load via `clap_juce_extensions` when building with `-DTRANSDUCER_BUILD_JUCE_CLAP=ON`.

### Recommended MPE Synths & Pitch-Bend Range

Set your receiving synth's **MPE Pitch Bend Range** to match the transducer's `bend_range` parameter (both CLAP `CLAP_NOTE_EXPRESSION_TUNING` and MIDI 2.0/MPE RPN `0x00/0x00` send this automatically, but some synths require manual MPE mode activation):

- **±48 semitones (MPE standard — Presets 1–4):**
  - **Bitwig Studio (Polymer / Phase-4 / Polysynth):** Enable *MPE* on the track/device inspector; defaults to `±48 st`.
  - **Surge XT / Vital:** Enable *MPE* mode; set MPE Pitch Bend Range to `48`.
  - **u-he Diva / Zebra2 / ZebraHZ:** Enable *MPE* in the MIDI table / configuration; set Pitch Bend Range to `48`.
  - **Xfer Serum / Arturia Pigments:** Enable *MPE* in Global/Settings; verify Bend Range = `48`.
- **±24 semitones (Tight Tracking — Preset 5):**
  - Best for **monophonic lead synths** or **classic VA patches** where `±48 st` feels too coarse over 14-bit MIDI pitch wheel (`±24 st` doubles pitch-wheel resolution while still covering 2-octave slides). Set the synth's bend range to `24`.

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
