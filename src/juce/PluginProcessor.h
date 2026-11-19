#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <transducer/dsp/string_pipeline.h>
#include <transducer/voice/voice_allocator.h>
#include <transducer/presets/factory_presets.h>

#include <array>
#include <atomic>
#include <cstdint>

class GuitarTransducerAudioProcessor final : public juce::AudioProcessor {
public:
    static constexpr int kStateVersion = 1;
    static constexpr size_t kMaxStrings = 6;

    GuitarTransducerAudioProcessor();
    ~GuitarTransducerAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Lock-free audio -> GUI telemetry (written with memory_order_relaxed on audio thread)
    struct Telemetry {
        std::atomic<int> last_active_string{-1};
        std::atomic<int> midi_note{-1};
        std::atomic<float> cents_offset{0.0f};
        std::atomic<float> pressure{0.0f};
        std::atomic<uint8_t> string_mask{0};
        std::atomic<uint8_t> active_channels{1};
    };

    juce::AudioProcessorValueTreeState& apvts() noexcept { return apvts_; }
    const Telemetry& telemetry() const noexcept { return telemetry_; }
    Telemetry& mutableTelemetry() noexcept { return telemetry_; }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts_;

    // Cached atomic parameter pointers (read lock-free on the audio thread)
    std::atomic<float>* bend_range_param_{nullptr};
    std::atomic<float>* transient_sens_param_{nullptr};
    std::atomic<float>* gate_thresh_param_{nullptr};
    std::atomic<float>* routing_mode_param_{nullptr};
    std::atomic<float>* output_dialect_param_{nullptr};

    // Shared DSP core (zero duplication with CLAP)
    std::array<transducer::dsp::StringPipeline, kMaxStrings> strings_{};
    std::array<int, kMaxStrings> string_midi_ch_{}; // 1-based MIDI channel (2..16 for MPE)
    std::array<int, kMaxStrings> string_note_{};
    std::array<float, kMaxStrings> string_cents_{};
    std::array<float, kMaxStrings> string_pressure_{};
    std::array<float, 16> rpn_sent_range_{};
    int next_mono_mpe_ch_{2}; // MPE member channels 2..16 (channel 1 is master)
    int current_program_{0};

    transducer::voice::VoiceAllocator voice_allocator_{};
    juce::MidiBuffer scratch_midi_{};
    Telemetry telemetry_{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTransducerAudioProcessor)
};
