#pragma once

#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

class LiveTelemetryMeter final : public juce::Component, private juce::Timer {
public:
    explicit LiveTelemetryMeter(const GuitarTransducerAudioProcessor::Telemetry& telemetry)
        : telemetry_(telemetry)
    {
        setOpaque(true);
        startTimerHz(30);
    }

    ~LiveTelemetryMeter() override { stopTimer(); }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.fillAll(juce::Colour(0xFF14171C));

        g.setColour(juce::Colour(0xFF232832));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.0f);

        auto area = getLocalBounds().reduced(14, 12);
        auto string_strip = area.removeFromBottom(20);
        area.removeFromBottom(8);

        auto left = area.removeFromLeft(130);
        auto right = area.removeFromRight(150);
        auto center = area.reduced(12, 0);

        const auto accent = juce::Colour(0xFF38BDF8);
        const auto text_main = juce::Colour(0xFFF3F4F6);
        const auto text_dim = juce::Colour(0xFF9CA3AF);
        const auto track_bg = juce::Colour(0xFF0D0F12);

        // 1. Pitch readout (MIDI note + name)
        g.setColour(text_dim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText("PITCH", left.removeFromTop(14), juce::Justification::centredLeft);

        g.setColour(midi_note_ >= 0 ? text_main : text_dim);
        g.setFont(juce::FontOptions(26.0f).withStyle("Bold"));
        g.drawText(formatNote(midi_note_), left, juce::Justification::centredLeft);

        // 2. Continuous cents offset (-50 .. +50 c)
        g.setColour(text_dim);
        g.setFont(juce::FontOptions(11.0f));
        auto cents_header = center.removeFromTop(14);
        g.drawText("BEND / CENTS", cents_header, juce::Justification::centredLeft);

        juce::String cents_str = (midi_note_ >= 0)
            ? juce::String::formatted("%+.1f c", cents_)
            : juce::String("--");
        g.setColour(midi_note_ >= 0 ? accent : text_dim);
        g.drawText(cents_str, cents_header, juce::Justification::centredRight);

        auto bar = center.withSizeKeepingCentre(center.getWidth(), 10).toFloat();
        g.setColour(track_bg);
        g.fillRoundedRectangle(bar, 4.0f);

        const float mid_x = bar.getCentreX();
        g.setColour(juce::Colour(0xFF2E3440));
        g.fillRect(mid_x - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);

        if (midi_note_ >= 0) {
            const float norm = std::clamp(cents_ / 50.0f, -1.0f, 1.0f);
            const float half_w = bar.getWidth() * 0.5f;
            const float fill_w = std::abs(norm) * half_w;
            const float fill_x = (norm >= 0.0f) ? mid_x : (mid_x - fill_w);
            g.setColour(accent);
            g.fillRoundedRectangle(fill_x, bar.getY(), std::max(fill_w, 2.0f), bar.getHeight(), 3.0f);
        }

        // 3. Pressure / envelope meter
        g.setColour(text_dim);
        g.setFont(juce::FontOptions(11.0f));
        auto press_hdr = right.removeFromTop(14);
        g.drawText("PRESSURE", press_hdr, juce::Justification::centredLeft);
        g.drawText(juce::String(static_cast<int>(std::round(pressure_ * 100.0f))) + "%",
                   press_hdr, juce::Justification::centredRight);

        auto pbar = right.withSizeKeepingCentre(right.getWidth(), 10).toFloat();
        g.setColour(track_bg);
        g.fillRoundedRectangle(pbar, 4.0f);
        if (pressure_ > 0.005f) {
            g.setColour(accent.withAlpha(0.85f));
            g.fillRoundedRectangle(pbar.getX(), pbar.getY(), pbar.getWidth() * pressure_, pbar.getHeight(), 4.0f);
        }

        // 4. Per-string / voice activity indicators (S1..S6)
        const int cell_w = string_strip.getWidth() / 6;
        g.setFont(juce::FontOptions(10.5f));
        for (int i = 0; i < 6; ++i) {
            auto cell = string_strip.withX(string_strip.getX() + i * cell_w).withWidth(cell_w - 4);
            const bool available = (i < active_channels_);
            const bool active = available && ((string_mask_ & static_cast<uint8_t>(1u << i)) != 0);

            g.setColour(active ? accent.withAlpha(0.22f) : track_bg);
            g.fillRoundedRectangle(cell.toFloat(), 3.0f);
            g.setColour(active ? accent : (available ? text_dim : juce::Colour(0xFF374151)));
            g.drawRoundedRectangle(cell.toFloat(), 3.0f, 1.0f);
            g.drawText("S" + juce::String(i + 1), cell, juce::Justification::centred);
        }
    }

private:
    void timerCallback() override
    {
        const int note = telemetry_.midi_note.load(std::memory_order_relaxed);
        const float cents = telemetry_.cents_offset.load(std::memory_order_relaxed);
        const float press = telemetry_.pressure.load(std::memory_order_relaxed);
        const uint8_t mask = telemetry_.string_mask.load(std::memory_order_relaxed);
        const uint8_t ch = telemetry_.active_channels.load(std::memory_order_relaxed);

        if (note != midi_note_ || std::abs(cents - cents_) > 0.2f ||
            std::abs(press - pressure_) > 0.01f || mask != string_mask_ || ch != active_channels_)
        {
            midi_note_ = note;
            cents_ = cents;
            pressure_ = press;
            string_mask_ = mask;
            active_channels_ = ch;
            repaint();
        }
    }

    static juce::String formatNote(int midi)
    {
        if (midi < 0 || midi > 127) return "--";
        static constexpr std::array<const char*, 12> kNames{
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int octave = (midi / 12) - 1;
        return juce::String(kNames[static_cast<size_t>(midi % 12)]) + juce::String(octave)
             + " (" + juce::String(midi) + ")";
    }

    const GuitarTransducerAudioProcessor::Telemetry& telemetry_;
    int midi_note_{-1};
    float cents_{0.0f};
    float pressure_{0.0f};
    uint8_t string_mask_{0};
    uint8_t active_channels_{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveTelemetryMeter)
};
