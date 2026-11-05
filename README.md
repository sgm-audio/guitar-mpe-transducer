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

## Building

Requirements:
- CMake 3.24+
- Modern C++20 compiler (MSVC 2022, GCC 11+, Clang 14+)

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## License

MIT (c) 2026 SGM Audio
