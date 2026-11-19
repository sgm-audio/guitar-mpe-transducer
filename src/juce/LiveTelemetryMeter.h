#pragma once

#include "PluginProcessor.h"
#include "UiSliceHelpers.h"
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
        setOpaque(false);
        syncNow();
        startTimerHz(30);
    }

    ~LiveTelemetryMeter() override { stopTimer(); }

    void syncNow()
    {
        last_active_string_ = telemetry_.last_active_string.load(std::memory_order_relaxed);
        midi_note_ = telemetry_.midi_note.load(std::memory_order_relaxed);
        cents_ = telemetry_.cents_offset.load(std::memory_order_relaxed);
        pressure_ = telemetry_.pressure.load(std::memory_order_relaxed);
        string_mask_ = telemetry_.string_mask.load(std::memory_order_relaxed);
        active_channels_ = telemetry_.active_channels.load(std::memory_order_relaxed);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        if (area.isEmpty())
            return;

        // Bottom strip: S1..S6 cohesive metallic tabs
        auto stringStrip = area.removeFromBottom(21);
        area.removeFromBottom(8);

        // Top row: PITCH S3 + E3 (52) on left, BEND meter in middle, PRESSURE meter on right
        auto pitchCol = area.removeFromLeft(166);
        area.removeFromLeft(16);
        auto pressureCol = area.removeFromRight(265);
        area.removeFromRight(18);
        auto bendCol = area;

        const juce::Colour creamText(0xFFF5E6D3);
        const juce::Colour bronzeText(0xFFC4A882);
        const juce::Colour amberGlow(0xFFFF9A3C);
        const juce::Colour amberBright(0xFFFFD060);
        const juce::Colour moltenOrange(0xFFFF6B1A);
        const juce::Colour grooveDark(0xFF0A0806);
        const juce::Colour bronzeBorder(0xFF6B4A2F);
        const juce::Colour bronzeHighlight(0xFF8B5A3C);
        const juce::Colour tabDark(0xFF14100C);

        // 1. Pitch readout: compact PITCH S3 header + tight recessed backing pill for glowing E3 (52)
        {
            auto hdr = pitchCol.removeFromTop(14);
            const juce::String pitchHdr = (last_active_string_ >= 0)
                ? ("PITCH S" + juce::String(last_active_string_ + 1))
                : "PITCH";
            g.setFont(transducer::ui::makeUiFont(11.5f, false, true));
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawText(pitchHdr, hdr.translated(0, 1), juce::Justification::centredLeft, false);
            g.setColour(bronzeText);
            g.drawText(pitchHdr, hdr, juce::Justification::centredLeft, false);

            pitchCol.removeFromTop(2);
            const juce::String noteText = formatNote(midi_note_);
            const auto noteFont = transducer::ui::makeUiFont(27.0f, true, true);

            // Compute compact recessed backing pill width tightly around text (no giant empty box)
            juce::GlyphArrangement ga;
            ga.addLineOfText(noteFont, noteText, 0.0f, 0.0f);
            const float textW = std::max(54.0f, ga.getBoundingBox(0, -1, true).getWidth());
            const float pillW = std::min(static_cast<float>(pitchCol.getWidth()), textW + 14.0f);
            const float pillH = std::min(static_cast<float>(pitchCol.getHeight()), 30.0f);
            const juce::Rectangle<float> pill(
                static_cast<float>(pitchCol.getX()),
                static_cast<float>(pitchCol.getY()) + 1.0f,
                pillW,
                pillH);

            // Subtle compact recessed pill + soft inner amber ambient glow
            g.setColour(juce::Colour(0xC80B0806));
            g.fillRoundedRectangle(pill, 4.0f);
            if (midi_note_ >= 0) {
                juce::ColourGradient pillGlow(moltenOrange.withAlpha(0.16f), pill.getCentreX(), pill.getCentreY(),
                                              juce::Colours::transparentBlack, pill.getX(), pill.getCentreY(), true);
                g.setGradientFill(pillGlow);
                g.fillRoundedRectangle(pill.reduced(1.0f), 3.0f);
            }
            g.setColour(bronzeBorder.withAlpha(0.55f));
            g.drawRoundedRectangle(pill.reduced(0.5f), 4.0f, 0.9f);

            const auto textBox = pill.reduced(6.0f, 1.0f);
            if (midi_note_ >= 0) {
                transducer::ui::drawGlowingAmberText(
                    g, noteText, noteFont, textBox, juce::Justification::centredLeft,
                    juce::Colour(0xFFFFDE8A), juce::Colour(0xFFFF8C2A), moltenOrange);
            } else {
                g.setFont(noteFont);
                g.setColour(bronzeText.withAlpha(0.5f));
                g.drawText(noteText, textBox.toNearestInt(), juce::Justification::centredLeft, false);
            }
        }

        // Helper for sleek recessed glowing horizontal meter bar (6px track)
        auto drawGlowingTrack = [&](juce::Rectangle<int> col,
                                    const juce::String& title,
                                    const juce::String& valueStr,
                                    bool isBipolar,
                                    float normVal) {
            auto hdr = col.removeFromTop(16);
            g.setFont(transducer::ui::makeUiFont(12.0f, false, true));
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawText(title, hdr.translated(0, 1), juce::Justification::centredLeft, false);
            g.setColour(bronzeText);
            g.drawText(title, hdr, juce::Justification::centredLeft, false);

            g.setFont(transducer::ui::makeUiFont(12.5f, true, false));
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawText(valueStr, hdr.translated(0, 1), juce::Justification::centredRight, false);
            g.setColour(midi_note_ >= 0 || !isBipolar ? amberGlow : bronzeText);
            g.drawText(valueStr, hdr, juce::Justification::centredRight, false);

            // Sleek recessed groove track (6px tall) aligned cleanly with pitch readout
            const float trackH = 6.0f;
            const float trackY = static_cast<float>(col.getY()) + (static_cast<float>(col.getHeight()) - trackH) * 0.48f;
            const juce::Rectangle<float> track(
                static_cast<float>(col.getX()), trackY,
                static_cast<float>(col.getWidth()), trackH);

            // Recessed shadow + #0A0806 groove fill + thin #6B4A2F border
            g.setColour(grooveDark);
            g.fillRoundedRectangle(track, 3.0f);
            g.setColour(bronzeBorder.withAlpha(0.85f));
            g.drawRoundedRectangle(track.reduced(0.25f), 3.0f, 0.95f);

            // Subtle top inner groove shadow
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawHorizontalLine(static_cast<int>(track.getY() + 1.0f), track.getX() + 2.0f, track.getRight() - 2.0f);

            if (isBipolar) {
                const float midX = track.getCentreX();
                g.setColour(bronzeHighlight.withAlpha(0.45f));
                g.fillRect(midX - 0.5f, track.getY() - 1.5f, 1.0f, track.getHeight() + 3.0f);
            }

            // Compute active fill rectangle
            juce::Rectangle<float> fillRect;
            if (isBipolar) {
                if (midi_note_ >= 0 && std::abs(normVal) > 0.005f) {
                    const float clamped = std::clamp(normVal, -1.0f, 1.0f);
                    const float midX = track.getCentreX();
                    const float halfW = (track.getWidth() - 2.0f) * 0.5f;
                    const float w = std::max(4.0f, std::abs(clamped) * halfW);
                    const float fx = (clamped >= 0.0f) ? midX : (midX - w);
                    fillRect = juce::Rectangle<float>(fx, track.getY() + 0.8f, w, track.getHeight() - 1.6f);
                }
            } else {
                const float clamped = std::clamp(normVal, 0.0f, 1.0f);
                if (clamped > 0.005f) {
                    const float w = std::max(4.0f, (track.getWidth() - 2.0f) * clamped);
                    fillRect = juce::Rectangle<float>(track.getX() + 1.0f, track.getY() + 0.8f, w, track.getHeight() - 1.6f);
                }
            }

            if (!fillRect.isEmpty()) {
                // Soft outer molten bloom
                g.setColour(moltenOrange.withAlpha(0.22f));
                g.fillRoundedRectangle(fillRect.expanded(3.5f, 3.2f), 5.0f);
                g.setColour(moltenOrange.withAlpha(0.40f));
                g.fillRoundedRectangle(fillRect.expanded(1.2f, 1.2f), 3.5f);

                // Glowing #FF6B1A -> #FFD060 horizontal gradient fill
                juce::ColourGradient grad(moltenOrange, fillRect.getX(), fillRect.getCentreY(),
                                          amberBright, fillRect.getRight(), fillRect.getCentreY(), false);
                g.setGradientFill(grad);
                g.fillRoundedRectangle(fillRect, 2.2f);
            }
        };

        // 2. BEND meter (-50 .. +50 c)
        const juce::String centsStr = (midi_note_ >= 0)
            ? juce::String::formatted("%+.1f c", cents_)
            : juce::String("--");
        drawGlowingTrack(bendCol, "BEND", centsStr, true, cents_ / 50.0f);

        // 3. PRESSURE meter (0 .. 100%)
        const juce::String pressStr = juce::String(static_cast<int>(std::round(pressure_ * 100.0f))) + "%";
        drawGlowingTrack(pressureCol, "PRESSURE", pressStr, false, pressure_);

        // 4. String Tabs (S1..S6): cohesive strip with 5px gaps, slim metallic inactive, molten lava active
        const float gap = 5.0f;
        const float totalW = static_cast<float>(stringStrip.getWidth());
        const float tabW = (totalW - gap * 5.0f) / 6.0f;
        const auto& lavaImg = transducer::ui::UiAssets::get().activeTabLava;

        for (int i = 0; i < 6; ++i) {
            const float tx = static_cast<float>(stringStrip.getX()) + static_cast<float>(i) * (tabW + gap);
            const juce::Rectangle<float> cell(tx, static_cast<float>(stringStrip.getY()),
                                              tabW, static_cast<float>(stringStrip.getHeight()));

            const bool available = (i < active_channels_);
            const bool active = available && ((string_mask_ & static_cast<uint8_t>(1u << i)) != 0);
            const bool focused = active && (i == last_active_string_);
            const bool lit = focused || (active && last_active_string_ < 0);

            if (lit) {
                // Soft fiery molten outer glow
                g.setColour(moltenOrange.withAlpha(0.36f));
                g.fillRoundedRectangle(cell.expanded(2.5f, 1.8f), 4.5f);

                // Clip molten lava fill cleanly inside the exact slim beveled pill bounds (no thick rivets)
                {
                    juce::Graphics::ScopedSaveState state(g);
                    juce::Path pillClip;
                    pillClip.addRoundedRectangle(cell.reduced(0.5f), 3.0f);
                    g.reduceClipRegion(pillClip);

                    if (lavaImg.isValid()) {
                        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                        g.drawImage(lavaImg, cell, juce::RectanglePlacement::stretchToFit);
                    } else {
                        juce::ColourGradient lavaGrad(moltenOrange, cell.getX(), cell.getCentreY(),
                                                      amberBright, cell.getCentreX(), cell.getCentreY(), true);
                        g.setGradientFill(lavaGrad);
                        g.fillRect(cell);
                    }

                    // Luminous warm center glow so S3 blazes like molten gold-orange magma
                    juce::ColourGradient hotCenter(juce::Colour(0x40FFF4B0), cell.getCentreX(), cell.getCentreY(),
                                                   juce::Colour(0x28FF4500), cell.getX(), cell.getY(), true);
                    g.setGradientFill(hotCenter);
                    g.fillRect(cell);
                }

                // Slim glowing amber bevel border
                g.setColour(amberBright.withAlpha(0.85f));
                g.drawRoundedRectangle(cell.reduced(0.5f), 3.0f, 1.0f);

                // Bold #0A0806 active tab text
                g.setFont(transducer::ui::makeUiFont(13.0f, true, false));
                g.setColour(grooveDark);
                g.drawText("S" + juce::String(i + 1), cell.toNearestInt(), juce::Justification::centred, false);
            } else {
                // Slim dark beveled metallic bar (#14100C fill, #6B4A2F border, #C4A882 text)
                juce::ColourGradient metalGrad(juce::Colour(0xFF1E1712), cell.getX(), cell.getY(),
                                               tabDark, cell.getX(), cell.getBottom(), false);
                g.setGradientFill(metalGrad);
                g.fillRoundedRectangle(cell.reduced(0.5f), 3.0f);

                // Top beveled highlight line
                g.setColour(bronzeHighlight.withAlpha(0.45f));
                g.drawHorizontalLine(static_cast<int>(cell.getY() + 1.0f), cell.getX() + 3.0f, cell.getRight() - 3.0f);

                // Border (subtly warmer if secondary active string)
                g.setColour(active ? amberGlow.withAlpha(0.65f) : bronzeBorder.withAlpha(0.85f));
                g.drawRoundedRectangle(cell.reduced(0.5f), 3.0f, 1.0f);

                g.setFont(transducer::ui::makeUiFont(12.0f, false, false));
                g.setColour(active ? creamText : (available ? bronzeText : bronzeText.withAlpha(0.45f)));
                g.drawText("S" + juce::String(i + 1), cell.toNearestInt(), juce::Justification::centred, false);
            }
        }
    }

private:
    void timerCallback() override
    {
        const int last_str = telemetry_.last_active_string.load(std::memory_order_relaxed);
        const int note = telemetry_.midi_note.load(std::memory_order_relaxed);
        const float cents = telemetry_.cents_offset.load(std::memory_order_relaxed);
        const float press = telemetry_.pressure.load(std::memory_order_relaxed);
        const uint8_t mask = telemetry_.string_mask.load(std::memory_order_relaxed);
        const uint8_t ch = telemetry_.active_channels.load(std::memory_order_relaxed);

        if (last_str != last_active_string_ || note != midi_note_ || std::abs(cents - cents_) > 0.2f ||
            std::abs(press - pressure_) > 0.01f || mask != string_mask_ || ch != active_channels_)
        {
            last_active_string_ = last_str;
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
    int last_active_string_{-1};
    int midi_note_{-1};
    float cents_{0.0f};
    float pressure_{0.0f};
    uint8_t string_mask_{0};
    uint8_t active_channels_{1};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LiveTelemetryMeter)
};
