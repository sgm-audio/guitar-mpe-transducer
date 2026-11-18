#pragma once

#include "PluginProcessor.h"
#include "LiveTelemetryMeter.h"
#include "TransducerLookAndFeel.h"

#include <jive_layouts/jive_layouts.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

class GuitarTransducerAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit GuitarTransducerAudioProcessorEditor(GuitarTransducerAudioProcessor&);
    ~GuitarTransducerAudioProcessorEditor() override;

    void resized() override;

private:
    static juce::ValueTree createLayoutTree();

    GuitarTransducerAudioProcessor& processor_;
    TransducerLookAndFeel look_and_feel_;
    jive::Interpreter interpreter_;
    juce::ValueTree layout_tree_;
    std::unique_ptr<jive::GuiItem> root_item_;
    LiveTelemetryMeter telemetry_meter_;
    juce::Component* telemetry_slot_{nullptr};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTransducerAudioProcessorEditor)
};
