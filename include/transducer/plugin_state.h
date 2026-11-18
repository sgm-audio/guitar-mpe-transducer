#pragma once

#include <transducer/dsp/string_pipeline.h>
#include <transducer/voice/voice_allocator.h>
#include <transducer/params/parameter_manager.h>
#include <transducer/common/thread_check.h>
#include <array>
#include <cstdint>

namespace transducer {

struct PluginState {
    static constexpr size_t kMaxStrings = 6;

    std::array<dsp::StringPipeline, kMaxStrings> strings{};
    std::array<uint8_t, kMaxStrings> string_channel{};
    std::array<float, 16> rpn_sent_range{};
    uint8_t next_mono_channel{0};

    voice::VoiceAllocator voice_allocator{};
    params::ParameterManager param_manager{};
    common::ThreadCheck thread_check{};

    double sample_rate{48000.0};
    uint32_t min_frames{32};
    uint32_t max_frames{512};

    float pitch_bend_range{48.0f};
    uint32_t output_dialect{0}; // 0 = CLAP notes, 1 = MIDI 2.0 UMP
    uint64_t dropped_events{0};

    void reset() {
        const float sr = static_cast<float>(sample_rate);
        for (size_t i = 0; i < kMaxStrings; ++i) {
            strings[i].prepare(sr, static_cast<int>(i));
            string_channel[i] = static_cast<uint8_t>(i);
        }
        rpn_sent_range.fill(-1.0f);
        next_mono_channel = 0;
        voice_allocator.reset();
        thread_check.reset_audio_thread();
        dropped_events = 0;
    }
};

} // namespace transducer
