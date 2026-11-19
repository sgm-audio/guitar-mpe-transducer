#include "../src/juce/PluginEditor.h"
#include "../src/juce/PluginProcessor.h"

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>
#include <cstring>
#include <iostream>
#include <memory>

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String outPath = "gui_preview.png";
    for (int i = 1; i < argc; ++i) {
        if ((std::strcmp(argv[i], "--out") == 0 || std::strcmp(argv[i], "-o") == 0) && i + 1 < argc) {
            outPath = argv[++i];
        } else if (argv[i][0] != '-') {
            outPath = argv[i];
        }
    }

    const juce::File outFile = juce::File::isAbsolutePath(outPath)
        ? juce::File(outPath)
        : juce::File::getCurrentWorkingDirectory().getChildFile(outPath);

    GuitarTransducerAudioProcessor processor;

    // Mocked parameter state matching target visual reference:
    // SENSITIVITY = 0.60, GATE = -48.0 dB, BEND RANGE = 24 st, Direct (Mono), CLAP Notes / MPE
    auto setParamPlain = [&](const char* id, float plainVal) {
        if (auto* param = processor.apvts().getParameter(id))
            param->setValueNotifyingHost(param->convertTo0to1(plainVal));
    };
    setParamPlain("transient_sensitivity", 0.001f + 0.60f * (0.05f - 0.001f));
    setParamPlain("gate_threshold", std::pow(10.0f, -48.0f / 20.0f));
    setParamPlain("bend_range", 24.0f);
    setParamPlain("routing_mode", 0.0f);
    setParamPlain("output_dialect", 0.0f);

    // Mocked live telemetry: S3, E3 (52), +14.5 c, 68%
    auto& t = processor.mutableTelemetry();
    t.last_active_string.store(2, std::memory_order_relaxed);
    t.midi_note.store(52, std::memory_order_relaxed);
    t.cents_offset.store(14.5f, std::memory_order_relaxed);
    t.pressure.store(0.68f, std::memory_order_relaxed);
    t.string_mask.store(static_cast<uint8_t>(1u << 2), std::memory_order_relaxed);
    t.active_channels.store(6, std::memory_order_relaxed);

    auto editor = std::make_unique<GuitarTransducerAudioProcessorEditor>(processor);
    editor->setBounds(0, 0, 1200, 675);

    // Pump the message loop briefly so JIVE finishes layout, styling, and parameter sync
    if (auto* mm = juce::MessageManager::getInstanceWithoutCreating())
        mm->runDispatchLoopUntil(50);

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
