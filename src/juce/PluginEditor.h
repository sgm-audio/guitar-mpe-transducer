#pragma once

#include "PluginProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

class GuitarTransducerAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit GuitarTransducerAudioProcessorEditor(GuitarTransducerAudioProcessor&);
    ~GuitarTransducerAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    GuitarTransducerAudioProcessor& processor_;
    juce::GenericAudioProcessorEditor generic_{processor_};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTransducerAudioProcessorEditor)
};
