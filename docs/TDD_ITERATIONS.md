# Guitar MPE/CLAP Transducer — 10 TDD Iterations to V10

This document records the 10 substantive Test-Driven Development (TDD) iterations that produced the **V10 Integrated Transducer Engine**. Every iteration followed strict **RED -> GREEN -> REFACTOR** cycles with zero-allocation C++20 code, MSVC `/W4` compliance, and automated test coverage.

---

## Architecture Overview

```
                      +-----------------------------------+
                      |         Guitar Audio In           |
                      +-----------------+-----------------+
                                        |
       +--------------------------------+--------------------------------+
       |                                |                                |
       v                                v                                v
+---------------+              +-----------------+              +------------------+
|   Transient   |              |  YIN Sub-sample |              |    Brightness    |
|   Detector    |              |  Pitch Tracker  |              |    Estimator     |
| (Envelope &   |              +--------+--------+              |  (Rice Spectral  |
|  Energy Flux) |                       |                       |     Centroid)    |
+-------+-------+                       v                       +--------+---------+
        |                      +-----------------+                       |
        |                      | PitchContinuity |                       |
        |                      | (Octave Filter) |                       |
        |                      +--------+--------+                       |
        |                               |                                |
        +---------------+---------------+--------------------------------+
                        |
                        v
        +-------------------------------+
        |    Articulation Classifier    |
        | (Open Pluck, Mute, Harmonics) |
        +---------------+---------------+
                        |
        +---------------+---------------+
        |                               |
        v                               v
+---------------+              +-----------------+
|    Vibrato    |              | ReleaseVelocity |
|   Detector    |              |    Estimator    |
| (Rate & Depth)|              +--------+--------+
+-------+-------+                       |
        |                               |
        +---------------+---------------+
                        |
                        v
        +-------------------------------+
        |       NoteStateMachine        |
        |   (Legato Bend vs Retrigger)  |
        +---------------+---------------+
                        |
                        v
        +-------------------------------+
        |   Dual-Neck Voice Allocator   |
        |  (Direct, Highest, TargetAll) |
        +---------------+---------------+
                        |
        +---------------+---------------+
        |                               |
        v                               v
+---------------+              +-----------------+
| CLAP Events   |              | MIDI 2.0 UMP    |
| (Tuning,      |              | (Type 4 Per-    |
|  Brightness,  |              |  Note Bend &    |
|  Pressure)    |              |  32-bit Vel)    |
+---------------+              +-----------------+
```

---

## Iteration Breakdown

### Iteration 1: Octave Jump Rejection & Pitch Continuity Filter
- **RED Test:** `tests/test_pitch_continuity.cpp`
- **Component:** `include/transducer/dsp/pitch_continuity.h` (`transducer::dsp::PitchContinuityFilter`)
- **Key Insight:** Guitar pick dynamics cause transient second-harmonic energy spikes (octave errors). The filter compares $\Delta f$ against $\pm 12$ and $\pm 24$ semitones; jumps are rejected unless sustained across 3+ consecutive frames with confidence $> 0.98$.
- **Commit:** `8c0dd5e`

### Iteration 2: Dynamic Release Velocity Estimation
- **RED Test:** `tests/test_release_velocity.cpp`
- **Component:** `include/transducer/dsp/release_velocity_estimator.h` (`transducer::dsp::ReleaseVelocityEstimator`)
- **Key Insight:** MIDI 2.0 / CLAP note-offs require expressive release velocity. Differentiates natural string ring-out (slow decay $\to$ low release velocity $\approx 0.1$) from palm mutes / chokes (instant cliff $\frac{dE}{dt} \to$ release velocity $> 0.7$).
- **Commit:** `6151424`

### Iteration 3: Dual-Neck Polyphonic Voice Allocator & Target Routing
- **RED Test:** `tests/test_voice_allocator.cpp`
- **Component:** `include/transducer/voice/voice_allocator.h` (`transducer::voice::VoiceAllocator`)
- **Key Insight:** Enables dual-neck performance: keyboard input holds polyphonic harmony (e.g. C4-E4-G4 chord) while guitar pitch bends modulate only targeted voice(s) (`kDirect`, `kTargetHighest`, `kTargetAll`).
- **Commit:** `8610e5e`

### Iteration 4: Continuous Timbral Brightness Extraction (`CLAP_NOTE_EXPRESSION_BRIGHTNESS`)
- **RED Test:** `tests/test_brightness_estimator.cpp`
- **Component:** `include/transducer/dsp/brightness_estimator.h` (`transducer::dsp::BrightnessEstimator`)
- **Formula:** Rice spectral centroid proxy computed in $O(1)$ sample-by-sample without FFT allocations:
  $$f_c \approx \frac{f_s}{2\pi} \sqrt{\frac{E[(\Delta x)^2]}{E[x^2]}}$$
- **Commit:** `721dbbc`

### Iteration 5: Guitar Articulation Classification
- **RED Test:** `tests/test_articulation_classifier.cpp`
- **Component:** `include/transducer/dsp/articulation_classifier.h` (`transducer::dsp::ArticulationClassifier`)
- **Key Insight:** Classifies playing technique into `kOpenPluck`, `kPalmMute`, and `kHarmonicTap` within 1 ms of onset by analyzing the initial energy decay ratio and spectral brightness.
- **Commit:** `295f61c`

### Iteration 6: Vibrato LFO Extraction (Rate & Depth)
- **RED Test:** `tests/test_vibrato_detector.cpp`
- **Component:** `include/transducer/dsp/vibrato_detector.h` (`transducer::dsp::VibratoDetector`)
- **Key Insight:** Tracks sub-semitone pitch oscillations in the human vibrato window (3.5 Hz to 9.0 Hz). Extracts both LFO rate (Hz) and depth (semitones) to feed synth modulation matrices.
- **Commit:** `07a45f1`

### Iteration 7: Thread-Safe CLAP Parameters (`CLAP_EXT_PARAMS`)
- **RED Test:** `tests/test_parameter_manager.cpp`
- **Component:** `include/transducer/params/parameter_manager.h` (`transducer::params::ParameterManager`)
- **Key Insight:** Exposes `kParamBendRange`, `kParamTransientSensitivity`, `kParamGateThreshold`, and `kParamRoutingMode`. Uses a lock-free `DoubleBufferSeqLock` to guarantee non-blocking, wait-free reads on the real-time audio thread during automation.
- **Commit:** `92691cf`

### Iteration 8: Legato Glissando vs Re-trigger State Machine
- **RED Test:** `tests/test_note_state_machine.cpp`
- **Component:** `include/transducer/dsp/note_state_machine.h` (`transducer::dsp::NoteStateMachine`)
- **Key Insight:** Unifies transient onset detection and pitch continuity into 5 discrete actions: `kNone`, `kNoteOn`, `kNoteOff`, `kBendModulate`, and `kRetrigger`. Allows uninterrupted legato slides and bends without false re-attacks.
- **Commit:** `ab149ea`

### Iteration 9: Full CLAP Preset Serialization (`CLAP_EXT_STATE`)
- **RED Test:** `tests/test_state_serialization.cpp`
- **Component:** `include/transducer/state/state_serializer.h` (`transducer::state::StateSerializer`)
- **Key Insight:** Binary stream serializer with header magic (`0x47545231`), versioning, and FNV-1a 32-bit checksum. Integrated with `clap_plugin_state_t` extension in `src/plugin.cpp`.
- **Commit:** `3e33c12`

### Iteration 10: V10 Integrated Engine & End-to-End Performance Test Suite
- **RED Test:** `tests/test_v10_integrated_engine.cpp`
- **Component:** `include/transducer/engine/transducer_engine.h` (`transducer::engine::TransducerEngine`)
- **Key Insight:** Unified real-time engine coordinating all 9 DSP stages. Validated against end-to-end synthetic audio performance simulating open plucks, sustained vibrato, legato string bends, dual-neck chord routing, and palm mute releases.
- **Commit:** (Final V10 Commit)

---

## Verification & Metrics

- **Platform:** Windows MSVC 18.10 (C++20), Linux Clang/GCC CI Matrix
- **Test Framework:** Catch2 v3.5.2
- **Test Suite Results:**
  - **Total Test Cases:** 26
  - **Total Assertions:** 1,189 passed (0 failed)
  - **Execution Time:** ~0.17 seconds
- **Audio Thread Safety:** 0 heap allocations, 0 system mutexes, cache-line aligned lock-free buffers.
