#pragma once

#include "PluginProcessor.h"
#include "TransducerLookAndFeel.h"
#include "VolcanicComponents.h"

#include <jive_layouts/jive_layouts.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

class GuitarTransducerAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                                   private juce::Timer {
public:
    explicit GuitarTransducerAudioProcessorEditor(GuitarTransducerAudioProcessor&);
    ~GuitarTransducerAudioProcessorEditor() override;

    void resized() override;

    void refreshTelemetryForPreview();

private:
    static juce::ValueTree createLayoutTree();
    void registerCustomComponents();
    void bindControlsAfterInterpret(juce::Component& rootComp);
    void timerCallback() override;

    GuitarTransducerAudioProcessor& processor_;
    TransducerLookAndFeel look_and_feel_;
    transducer::ui::TelemetrySnapshot telemetry_snapshot_;

    jive::Interpreter interpreter_;
    juce::ValueTree layout_tree_;
    std::unique_ptr<jive::GuiItem> root_item_;

    juce::ComboBox* preset_combo_{nullptr};
    juce::Component* pitch_view_{nullptr};
    juce::Component* bend_view_{nullptr};
    juce::Component* pressure_view_{nullptr};
    std::array<juce::Component*, 6> string_tab_views_{};

    transducer::ui::ChassisOverlay chassis_overlay_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTransducerAudioProcessorEditor)
};
