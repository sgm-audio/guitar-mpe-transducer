#pragma once

#include "PluginProcessor.h"
#include "LiveTelemetryMeter.h"
#include "TransducerLookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>

class GuitarTransducerAudioProcessorEditor final : public juce::AudioProcessorEditor {
public:
    explicit GuitarTransducerAudioProcessorEditor(GuitarTransducerAudioProcessor&);
    ~GuitarTransducerAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshTelemetryForPreview() { telemetry_meter_.syncNow(); }

private:
    class FrameOverlay final : public juce::Component {
    public:
        FrameOverlay()
        {
            setInterceptsMouseClicks(false, false);
            setOpaque(false);
        }

        void paint(juce::Graphics& g) override
        {
            const auto& frameImg = transducer::ui::UiAssets::get().chassisFrame;
            if (frameImg.isValid()) {
                // 9-slice with uniform 1:1 corner scaling so every outer rivet stays perfectly circular.
                transducer::ui::drawNineSlice(
                    g, frameImg, getLocalBounds().toFloat(),
                    juce::BorderSize<int>(156, 172, 156, 172), 0.36f);
            }
        }
    };

    void configureRotarySlider(juce::Slider& slider,
                               const juce::String& knobStyle,
                               std::function<juce::String(double)> textFromValue,
                               std::function<double(const juce::String&)> valueFromText);

    GuitarTransducerAudioProcessor& processor_;
    TransducerLookAndFeel look_and_feel_;

    LiveTelemetryMeter telemetry_meter_;
    juce::ComboBox preset_combo_;
    juce::Slider sensitivity_slider_;
    juce::Slider gate_slider_;
    juce::Slider bend_range_slider_;
    juce::ComboBox routing_combo_;
    juce::ComboBox output_combo_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensitivity_attachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gate_attachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bend_range_attachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> routing_attachment_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> output_attachment_;

    FrameOverlay frame_overlay_;

    // Cached layout rectangles computed in resized() for clean paint() alignment
    juce::Rectangle<int> title_bounds_;
    juce::Rectangle<int> badge_bounds_;
    juce::Rectangle<int> detection_panel_bounds_;
    juce::Rectangle<int> detection_header_bounds_;
    juce::Rectangle<int> detection_body_bounds_;
    juce::Rectangle<int> knob_label_bounds_[3];
    juce::Rectangle<int> routing_panel_bounds_;
    juce::Rectangle<int> routing_header_bounds_;
    juce::Rectangle<int> routing_label_bounds_;
    juce::Rectangle<int> output_label_bounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GuitarTransducerAudioProcessorEditor)
};
