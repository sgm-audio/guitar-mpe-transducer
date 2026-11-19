#pragma once

#include "PluginProcessor.h"
#include "TransducerLookAndFeel.h"
#include "UiSliceHelpers.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace transducer::ui {

/**
 * Shared snapshot of processor telemetry consumed by JIVE-managed custom
 * telemetry views (PitchReadoutView, TelemetryBarView, StringTabView).
 */
struct TelemetrySnapshot {
    int lastActiveString{-1};
    int midiNote{-1};
    float cents{0.0f};
    float pressure{0.0f};
    uint8_t stringMask{0};
    uint8_t activeChannels{1};

    static TelemetrySnapshot load(const GuitarTransducerAudioProcessor::Telemetry& t) noexcept
    {
        TelemetrySnapshot s;
        s.lastActiveString = t.last_active_string.load(std::memory_order_relaxed);
        s.midiNote = t.midi_note.load(std::memory_order_relaxed);
        s.cents = t.cents_offset.load(std::memory_order_relaxed);
        s.pressure = t.pressure.load(std::memory_order_relaxed);
        s.stringMask = t.string_mask.load(std::memory_order_relaxed);
        s.activeChannels = t.active_channels.load(std::memory_order_relaxed);
        return s;
    }

    bool differsFrom(const TelemetrySnapshot& other) const noexcept
    {
        return lastActiveString != other.lastActiveString
            || midiNote != other.midiNote
            || std::abs(cents - other.cents) > 0.2f
            || std::abs(pressure - other.pressure) > 0.01f
            || stringMask != other.stringMask
            || activeChannels != other.activeChannels;
    }

    static juce::String formatNote(int midi)
    {
        if (midi < 0 || midi > 127)
            return "--";
        static constexpr std::array<const char*, 12> kNames{
            "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
        };
        const int octave = (midi / 12) - 1;
        return juce::String(kNames[static_cast<size_t>(midi % 12)])
             + juce::String(octave) + " (" + juce::String(midi) + ")";
    }
};

/**
 * Root JIVE container painting the dark volcanic obsidian well behind all
 * JIVE-laid-out panels and controls.
 */
class VolcanicRoot final : public juce::Component {
public:
    VolcanicRoot() { setOpaque(true); }

    void paint(juce::Graphics& g) override
    {
        const auto& assets = UiAssets::get();
        const auto fullBounds = getLocalBounds().toFloat();

        g.fillAll(juce::Colour(0xFF0A0806));
        if (assets.lavaRockTile.isValid()) {
            juce::Graphics::ScopedSaveState state(g);
            const auto innerWell = fullBounds.reduced(54.0f, 46.0f);
            g.reduceClipRegion(innerWell.toNearestInt());
            g.setTiledImageFill(assets.lavaRockTile, 0, 0, 0.72f);
            g.fillRect(innerWell);

            // Subtle dark wash so upper header, telemetry, and routing stay high-contrast
            g.setColour(juce::Colour(0x9A0A0806));
            g.fillRect(innerWell);

            juce::ColourGradient outerVig(juce::Colours::transparentBlack,
                                          innerWell.getCentreX(), innerWell.getCentreY(),
                                          juce::Colour(0x99050403),
                                          innerWell.getX(), innerWell.getY(), true);
            g.setGradientFill(outerVig);
            g.fillRect(innerWell);
        }
    }
};

/**
 * JIVE-managed custom title renderer: bold vertical warm amber-orange gradient
 * (#FFDE8A -> #FF6B1A) with subtle 2-pass #FF6B1A outer glow and drop shadow.
 */
class GlowingTitle final : public juce::Component {
public:
    GlowingTitle()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        const float fontPt = std::clamp(bounds.getHeight() * 0.90f, 22.0f, 36.0f);
        const auto titleFont = makeUiFont(fontPt, true, true);
        drawGlowingAmberText(g,
                             "GUITAR MPE TRANSDUCER",
                             titleFont,
                             bounds,
                             juce::Justification::centredLeft,
                             juce::Colour(0xFFFFDE8A),
                             juce::Colour(0xFFFF6B1A),
                             juce::Colour(0xFFFF6B1A));
    }
};

/**
 * JIVE-managed container for DETECTION & TRACKING: renders outer grounded drop
 * shadow in paint() and slim beveled oxidized bronze border (#6B4A2F..#8B5A3C)
 * in paintOverChildren() so the bronze header strip and tiled lava bay sit
 * cleanly inside the slim metallic frame with zero corner rivets or lava seams.
 */
class DetectionPanel final : public juce::Component {
public:
    DetectionPanel() { setOpaque(false); }

    void paint(juce::Graphics& g) override
    {
        const auto panelF = getLocalBounds().toFloat();
        g.setColour(juce::Colours::black.withAlpha(0.70f));
        g.fillRoundedRectangle(panelF.expanded(1.5f).translated(0.0f, 2.0f), 6.0f);
    }

    void paintOverChildren(juce::Graphics& g) override
    {
        const auto panelF = getLocalBounds().toFloat();
        juce::ColourGradient outerBevel(TransducerLookAndFeel::kBronzeLight,
                                        panelF.getX(), panelF.getY(),
                                        TransducerLookAndFeel::kBronzeBorder.darker(0.35f),
                                        panelF.getX(), panelF.getBottom(), false);
        g.setGradientFill(outerBevel);
        g.drawRoundedRectangle(panelF.reduced(1.6f), 5.0f, 3.4f);

        g.setColour(juce::Colour(0xCC0A0705));
        g.drawRoundedRectangle(panelF.reduced(3.4f), 3.8f, 1.0f);

        g.setColour(juce::Colour(0x44E0B088));
        g.drawRoundedRectangle(panelF.reduced(0.5f), 5.5f, 0.85f);
    }
};

/**
 * JIVE-managed weathered oxidized bronze header strip (~28px tall) at the top
 * of DETECTION & TRACKING. Its child <Text> is positioned by JIVE Flex.
 */
class DetectionHeader final : public juce::Component {
public:
    DetectionHeader() { setOpaque(false); }

    void paint(juce::Graphics& g) override
    {
        const auto headerF = getLocalBounds().toFloat();
        const auto& assets = UiAssets::get();

        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(headerF.getX() + 2.0f, headerF.getY() + 2.0f,
                                 headerF.getWidth() - 4.0f, headerF.getHeight() - 2.0f,
                                 4.0f, 4.0f, true, true, false, false);
        g.reduceClipRegion(clip);

        if (assets.bronzeHeaderStrip.isValid()) {
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            g.drawImage(assets.bronzeHeaderStrip, headerF, juce::RectanglePlacement::stretchToFit);
        } else {
            juce::ColourGradient hdrGrad(juce::Colour(0xFF6B4A2F), headerF.getX(), headerF.getY(),
                                         juce::Colour(0xFF3E2918), headerF.getX(), headerF.getBottom(), false);
            g.setGradientFill(hdrGrad);
            g.fillRect(headerF);
        }

        // Subtle metallic bevel sheen & dark bottom separator
        juce::ColourGradient hdrSheen(juce::Colour(0x28FFD8B0), headerF.getX(), headerF.getY(),
                                      juce::Colour(0x460A0604), headerF.getX(), headerF.getBottom(), false);
        g.setGradientFill(hdrSheen);
        g.fillRect(headerF);

        g.setColour(juce::Colour(0xBB0A0705));
        g.drawHorizontalLine(static_cast<int>(headerF.getBottom()) - 1, headerF.getX(), headerF.getRight());
        g.setColour(TransducerLookAndFeel::kBronzeLight.withAlpha(0.55f));
        g.drawHorizontalLine(static_cast<int>(headerF.getY()) + 3, headerF.getX() + 4.0f, headerF.getRight() - 4.0f);
    }
};

/**
 * JIVE-managed volcanic lava-rock interior bay for the three hero knobs.
 * Renders seamlessly tiled basalt rock + radial vignette + recessed inner shadows.
 */
class DetectionBay final : public juce::Component {
public:
    DetectionBay() { setOpaque(false); }

    void paint(juce::Graphics& g) override
    {
        const auto bodyF = getLocalBounds().toFloat();
        const auto& assets = UiAssets::get();

        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(bodyF.getX() + 2.0f, bodyF.getY(),
                                 bodyF.getWidth() - 4.0f, bodyF.getHeight() - 2.0f,
                                 4.0f, 4.0f, false, false, true, true);
        g.reduceClipRegion(clip);

        if (assets.lavaRockTile.isValid()) {
            g.setTiledImageFill(assets.lavaRockTile, 0, 0, 1.0f);
            g.fillRect(bodyF);
        } else {
            g.setColour(juce::Colour(0xFF14100C));
            g.fillRect(bodyF);
        }

        // Subtle dark radial vignette so volcanic basalt stays richly textured behind knobs
        juce::ColourGradient vignette(juce::Colour(0x04000000),
                                      bodyF.getCentreX(), bodyF.getCentreY(),
                                      juce::Colour(0x78060403),
                                      bodyF.getX(), bodyF.getY(), true);
        g.setGradientFill(vignette);
        g.fillRect(bodyF);

        // Recessed top/bottom inner shadows
        juce::ColourGradient topShadow(juce::Colour(0xAA000000), bodyF.getX(), bodyF.getY(),
                                       juce::Colours::transparentBlack, bodyF.getX(), bodyF.getY() + 16.0f, false);
        g.setGradientFill(topShadow);
        g.fillRect(bodyF.getX(), bodyF.getY(), bodyF.getWidth(), 16.0f);

        juce::ColourGradient botShadow(juce::Colours::transparentBlack, bodyF.getX(), bodyF.getBottom() - 16.0f,
                                       juce::Colour(0x99000000), bodyF.getX(), bodyF.getBottom(), false);
        g.setGradientFill(botShadow);
        g.fillRect(bodyF.getX(), bodyF.getBottom() - 16.0f, bodyF.getWidth(), 16.0f);
    }
};

/**
 * JIVE-managed ROUTING & OUTPUT panel with slim (2.5px) recessed oxidized
 * bronze outline matching the inner panel design language.
 */
class RoutingPanel final : public juce::Component {
public:
    RoutingPanel() { setOpaque(false); }

    void paint(juce::Graphics& g) override
    {
        const auto rf = getLocalBounds().toFloat();

        g.setColour(juce::Colour(0xBC0E0A08));
        g.fillRoundedRectangle(rf.reduced(1.0f), 4.5f);

        // Subtle top inner recessed shadow
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(rf.reduced(1.5f), 4.0f);
            g.reduceClipRegion(clip);
            juce::ColourGradient topShad(juce::Colour(0x88000000), rf.getX(), rf.getY(),
                                         juce::Colours::transparentBlack, rf.getX(), rf.getY() + 11.0f, false);
            g.setGradientFill(topShad);
            g.fillRect(rf.getX(), rf.getY(), rf.getWidth(), 11.0f);
        }

        // Slim 2.5px beveled oxidized bronze outline
        juce::ColourGradient routeBorder(TransducerLookAndFeel::kBronzeLight.withAlpha(0.85f),
                                         rf.getX(), rf.getY(),
                                         TransducerLookAndFeel::kBronzeBorder.darker(0.30f),
                                         rf.getX(), rf.getBottom(), false);
        g.setGradientFill(routeBorder);
        g.drawRoundedRectangle(rf.reduced(1.25f), 4.5f, 2.5f);

        g.setColour(juce::Colour(0xAA080604));
        g.drawRoundedRectangle(rf.reduced(2.7f), 3.6f, 0.9f);
    }
};

/**
 * JIVE-managed Pitch Readout widget: "PITCH S3" header + glowing amber-gold
 * "E3 (52)" tucked inside a compact recessed backing pill.
 */
class PitchReadoutView final : public juce::Component {
public:
    explicit PitchReadoutView(const TelemetrySnapshot& snapshot) : snapshot_(snapshot)
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        if (area.isEmpty())
            return;

        auto hdr = area.removeFromTop(16);
        const juce::String pitchHdr = (snapshot_.lastActiveString >= 0)
            ? ("PITCH S" + juce::String(snapshot_.lastActiveString + 1))
            : "PITCH";

        g.setFont(makeUiFont(13.5f, false, true));
        g.setColour(juce::Colours::black.withAlpha(0.78f));
        g.drawText(pitchHdr, hdr.translated(0, 1), juce::Justification::centredLeft, false);
        g.setColour(TransducerLookAndFeel::kBronzeText);
        g.drawText(pitchHdr, hdr, juce::Justification::centredLeft, false);

        area.removeFromTop(2);
        const juce::String noteText = TelemetrySnapshot::formatNote(snapshot_.midiNote);
        const auto noteFont = makeUiFont(31.5f, true, true);

        juce::GlyphArrangement ga;
        ga.addLineOfText(noteFont, noteText, 0.0f, 0.0f);
        const float textW = std::max(64.0f, ga.getBoundingBox(0, -1, true).getWidth());
        const float pillW = std::min(static_cast<float>(area.getWidth()), textW + 16.0f);
        const float pillH = std::min(static_cast<float>(area.getHeight()), 34.0f);
        const juce::Rectangle<float> pill(
            static_cast<float>(area.getX()),
            static_cast<float>(area.getY()) + 1.0f,
            pillW,
            pillH);

        // Compact recessed backing pill + soft inner amber ambient glow
        g.setColour(juce::Colour(0xC80B0806));
        g.fillRoundedRectangle(pill, 4.0f);
        if (snapshot_.midiNote >= 0) {
            juce::ColourGradient pillGlow(TransducerLookAndFeel::kMoltenOrange.withAlpha(0.16f),
                                          pill.getCentreX(), pill.getCentreY(),
                                          juce::Colours::transparentBlack,
                                          pill.getX(), pill.getCentreY(), true);
            g.setGradientFill(pillGlow);
            g.fillRoundedRectangle(pill.reduced(1.0f), 3.0f);
        }
        g.setColour(TransducerLookAndFeel::kBronzeBorder.withAlpha(0.36f));
        g.drawRoundedRectangle(pill.reduced(0.5f), 4.0f, 0.85f);

        const auto textBox = pill.reduced(7.0f, 1.0f);
        if (snapshot_.midiNote >= 0) {
            drawGlowingAmberText(g, noteText, noteFont, textBox, juce::Justification::centredLeft,
                                 juce::Colour(0xFFFFDE8A),
                                 juce::Colour(0xFFFF8C2A),
                                 TransducerLookAndFeel::kMoltenOrange);
        } else {
            g.setFont(noteFont);
            g.setColour(TransducerLookAndFeel::kBronzeText.withAlpha(0.5f));
            g.drawText(noteText, textBox.toNearestInt(), juce::Justification::centredLeft, false);
        }
    }

private:
    const TelemetrySnapshot& snapshot_;
};

/**
 * JIVE-managed glowing horizontal telemetry bar (BEND bipolar or PRESSURE unipolar).
 */
class TelemetryBarView final : public juce::Component {
public:
    enum class Mode { bend, pressure };

    TelemetryBarView(const TelemetrySnapshot& snapshot, Mode mode)
        : snapshot_(snapshot), mode_(mode)
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        auto col = getLocalBounds();
        if (col.isEmpty())
            return;

        const bool isBipolar = (mode_ == Mode::bend);
        const juce::String title = isBipolar ? "BEND" : "PRESSURE";
        const juce::String valueStr = isBipolar
            ? (snapshot_.midiNote >= 0 ? juce::String::formatted("%+.1f c", snapshot_.cents) : juce::String("--"))
            : (juce::String(static_cast<int>(std::round(snapshot_.pressure * 100.0f))) + "%");
        const float normVal = isBipolar ? (snapshot_.cents / 50.0f) : snapshot_.pressure;

        auto hdr = col.removeFromTop(18);
        g.setFont(makeUiFont(14.0f, false, true));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText(title, hdr.translated(0, 1), juce::Justification::centredLeft, false);
        g.setColour(TransducerLookAndFeel::kBronzeText);
        g.drawText(title, hdr, juce::Justification::centredLeft, false);

        g.setFont(makeUiFont(14.5f, true, false));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText(valueStr, hdr.translated(0, 1), juce::Justification::centredRight, false);
        g.setColour((snapshot_.midiNote >= 0 || !isBipolar) ? TransducerLookAndFeel::kAmberGlow
                                                            : TransducerLookAndFeel::kBronzeText);
        g.drawText(valueStr, hdr, juce::Justification::centredRight, false);

        // Sleek 7px recessed groove track (#0A0806 fill, #6B4A2F border)
        const float trackH = 7.0f;
        const float trackY = static_cast<float>(col.getY()) + (static_cast<float>(col.getHeight()) - trackH) * 0.46f;
        const juce::Rectangle<float> track(
            static_cast<float>(col.getX()), trackY,
            static_cast<float>(col.getWidth()), trackH);

        g.setColour(TransducerLookAndFeel::kGrooveDark);
        g.fillRoundedRectangle(track, 3.5f);
        g.setColour(TransducerLookAndFeel::kBronzeBorder.withAlpha(0.88f));
        g.drawRoundedRectangle(track.reduced(0.25f), 3.5f, 1.0f);

        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawHorizontalLine(static_cast<int>(track.getY() + 1.0f), track.getX() + 2.0f, track.getRight() - 2.0f);

        if (isBipolar) {
            const float midX = track.getCentreX();
            g.setColour(TransducerLookAndFeel::kBronzeLight.withAlpha(0.45f));
            g.fillRect(midX - 0.5f, track.getY() - 1.5f, 1.0f, track.getHeight() + 3.0f);
        }

        juce::Rectangle<float> fillRect;
        if (isBipolar) {
            if (snapshot_.midiNote >= 0 && std::abs(normVal) > 0.005f) {
                const float clamped = std::clamp(normVal, -1.0f, 1.0f);
                const float midX = track.getCentreX();
                const float halfW = (track.getWidth() - 2.0f) * 0.5f;
                const float w = std::max(4.5f, std::abs(clamped) * halfW);
                const float fx = (clamped >= 0.0f) ? midX : (midX - w);
                fillRect = juce::Rectangle<float>(fx, track.getY() + 0.9f, w, track.getHeight() - 1.8f);
            }
        } else {
            const float clamped = std::clamp(normVal, 0.0f, 1.0f);
            if (clamped > 0.005f) {
                const float w = std::max(4.5f, (track.getWidth() - 2.0f) * clamped);
                fillRect = juce::Rectangle<float>(track.getX() + 1.0f, track.getY() + 0.9f, w, track.getHeight() - 1.8f);
            }
        }

        if (!fillRect.isEmpty()) {
            g.setColour(TransducerLookAndFeel::kMoltenOrange.withAlpha(0.22f));
            g.fillRoundedRectangle(fillRect.expanded(4.0f, 3.5f), 5.5f);
            g.setColour(TransducerLookAndFeel::kMoltenOrange.withAlpha(0.42f));
            g.fillRoundedRectangle(fillRect.expanded(1.4f, 1.4f), 3.8f);

            juce::ColourGradient grad(TransducerLookAndFeel::kMoltenOrange, fillRect.getX(), fillRect.getCentreY(),
                                      juce::Colour(0xFFFFD060), fillRect.getRight(), fillRect.getCentreY(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(fillRect, 2.5f);
        }
    }

private:
    const TelemetrySnapshot& snapshot_;
    Mode mode_;
};

/**
 * JIVE-managed single string tab (S1..S6). Sized and spaced exclusively by JIVE
 * Flex layout (equal width, 5-6px gaps). Renders slim dark beveled metallic bar
 * when inactive and clean molten lava pill with bold #0A0806 text when active.
 */
class StringTabView final : public juce::Component {
public:
    explicit StringTabView(const TelemetrySnapshot& snapshot) : snapshot_(snapshot)
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        const auto cell = getLocalBounds().toFloat();
        if (cell.isEmpty())
            return;

        // Parse 1-based index from component ID ("string-tab-1" .. "string-tab-6")
        int idx = 0;
        const auto idStr = getComponentID();
        const int dash = idStr.lastIndexOfChar('-');
        if (dash >= 0)
            idx = std::clamp(idStr.substring(dash + 1).getIntValue() - 1, 0, 5);

        const bool available = (idx < snapshot_.activeChannels);
        const bool active = available && ((snapshot_.stringMask & static_cast<uint8_t>(1u << idx)) != 0);
        const bool focused = active && (idx == snapshot_.lastActiveString);
        const bool lit = focused || (active && snapshot_.lastActiveString < 0);

        const auto& lavaImg = UiAssets::get().activeTabLava;

        if (lit) {
            // Subtle fiery outer glow inside slight inset so adjacent tabs never clip awkwardly
            const auto pill = cell.reduced(0.5f);

            {
                juce::Graphics::ScopedSaveState state(g);
                juce::Path pillClip;
                pillClip.addRoundedRectangle(pill, 3.2f);
                g.reduceClipRegion(pillClip);

                if (lavaImg.isValid()) {
                    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                    g.drawImage(lavaImg, pill, juce::RectanglePlacement::stretchToFit);
                } else {
                    juce::ColourGradient lavaGrad(TransducerLookAndFeel::kMoltenOrange, pill.getX(), pill.getCentreY(),
                                                  juce::Colour(0xFFFFD060), pill.getCentreX(), pill.getCentreY(), true);
                    g.setGradientFill(lavaGrad);
                    g.fillRect(pill);
                }

                juce::ColourGradient hotCenter(juce::Colour(0x42FFF4B0), pill.getCentreX(), pill.getCentreY(),
                                               juce::Colour(0x28FF4500), pill.getX(), pill.getY(), true);
                g.setGradientFill(hotCenter);
                g.fillRect(pill);
            }

            // Slim glowing amber bevel border
            g.setColour(TransducerLookAndFeel::kAmberBright.withAlpha(0.88f));
            g.drawRoundedRectangle(pill, 3.2f, 1.15f);

            // Bold #0A0806 active label
            g.setFont(makeUiFont(15.0f, true, false));
            g.setColour(TransducerLookAndFeel::kGrooveDark);
            g.drawText("S" + juce::String(idx + 1), getLocalBounds(), juce::Justification::centred, false);
        } else {
            const auto pill = cell.reduced(0.5f);

            juce::ColourGradient metalGrad(juce::Colour(0xFF1F1812), pill.getX(), pill.getY(),
                                           TransducerLookAndFeel::kTabDark, pill.getX(), pill.getBottom(), false);
            g.setGradientFill(metalGrad);
            g.fillRoundedRectangle(pill, 3.2f);

            g.setColour(TransducerLookAndFeel::kBronzeLight.withAlpha(0.45f));
            g.drawHorizontalLine(static_cast<int>(pill.getY() + 1.0f), pill.getX() + 3.0f, pill.getRight() - 3.0f);

            g.setColour(active ? TransducerLookAndFeel::kAmberGlow.withAlpha(0.65f)
                               : TransducerLookAndFeel::kBronzeBorder.withAlpha(0.88f));
            g.drawRoundedRectangle(pill, 3.2f, 1.05f);

            g.setFont(makeUiFont(14.0f, false, false));
            g.setColour(active ? TransducerLookAndFeel::kCreamText
                               : (available ? TransducerLookAndFeel::kBronzeText
                                            : TransducerLookAndFeel::kBronzeText.withAlpha(0.45f)));
            g.drawText("S" + juce::String(idx + 1), getLocalBounds(), juce::Justification::centred, false);
        }
    }

private:
    const TelemetrySnapshot& snapshot_;
};

/**
 * Non-interactive 9-slice outer volcanic bronze chassis overlay.
 * Uses uniform corner scaling (borderScale = 0.42f at 1200x675) so every
 * riveted corner stays perfectly circular without oval distortion.
 */
class ChassisOverlay final : public juce::Component {
public:
    ChassisOverlay()
    {
        setInterceptsMouseClicks(false, false);
        setOpaque(false);
    }

    void paint(juce::Graphics& g) override
    {
        const auto& frameImg = UiAssets::get().chassisFrame;
        if (frameImg.isValid()) {
            drawNineSlice(g,
                          frameImg,
                          getLocalBounds().toFloat(),
                          juce::BorderSize<int>(156, 172, 156, 172),
                          0.42f);
        }
    }
};

} // namespace transducer::ui
