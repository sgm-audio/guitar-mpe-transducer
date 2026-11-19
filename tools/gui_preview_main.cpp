#include "../src/juce/PluginEditor.h"
#include "../src/juce/PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <iostream>
#include <memory>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outFile = (argc > 1)
        ? juce::File::getCurrentWorkingDirectory().getChildFile(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("gui_preview.png");

    GuitarTransducerAudioProcessor processor;

    // Set preview parameter values matching the reference layout:
    // SENSITIVITY = 0.60, GATE = -48.0 dB, BEND RANGE = 24 st, Direct (Mono), CLAP Notes / MPE
    auto setParamPlain = [&](const char* id, float plainVal) {
        if (auto* param = processor.apvts().getParameter(id)) {
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
        }
    };
    setParamPlain("transient_sensitivity", 0.001f + 0.60f * (0.05f - 0.001f));
    setParamPlain("gate_threshold", std::pow(10.0f, -48.0f / 20.0f));
    setParamPlain("bend_range", 24.0f);
    setParamPlain("routing_mode", 0.0f);
    setParamPlain("output_dialect", 0.0f);

    // Populate live preview telemetry: PITCH S3, E3 (52), BEND +14.5 c, PRESSURE 68%, S3 active
    auto& t = processor.mutableTelemetry();
    t.last_active_string.store(2, std::memory_order_relaxed);
    t.midi_note.store(52, std::memory_order_relaxed);
    t.cents_offset.store(14.5f, std::memory_order_relaxed);
    t.pressure.store(0.68f, std::memory_order_relaxed);
    t.string_mask.store(static_cast<uint8_t>(1u << 2), std::memory_order_relaxed);
    t.active_channels.store(6, std::memory_order_relaxed);

    auto editor = std::make_unique<GuitarTransducerAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, editor->getWidth(), editor->getHeight());
    editor->refreshTelemetryForPreview();

    constexpr float kScale = 2.0f;
    const int outW = static_cast<int>(std::round(editor->getWidth() * kScale));
    const int outH = static_cast<int>(std::round(editor->getHeight() * kScale));

    juce::Image snapshot(juce::Image::ARGB, outW, outH, true);
    {
        juce::Graphics g(snapshot);
        g.addTransform(juce::AffineTransform::scale(kScale));
        editor->paintEntireComponent(g, true);
    }

    if (outFile.existsAsFile())
        outFile.deleteFile();
    if (auto stream = outFile.createOutputStream()) {
        juce::PNGImageFormat png;
        if (!png.writeImageToStream(snapshot, *stream)) {
            std::cerr << "Failed to encode PNG to " << outFile.getFullPathName() << "\n";
            return 1;
        }
    } else {
        std::cerr << "Failed to open output file " << outFile.getFullPathName() << "\n";
        return 1;
    }

    std::cout << "Rendered " << outW << "x" << outH << " preview to "
              << outFile.getFullPathName() << "\n";
    return 0;
}
