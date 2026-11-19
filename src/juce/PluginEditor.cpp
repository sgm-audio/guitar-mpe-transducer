#include "PluginEditor.h"
#include "UiSliceHelpers.h"
#include <transducer/presets/factory_presets.h>
#include <algorithm>
#include <cmath>

namespace {
constexpr float kSensMin = 0.001f;
constexpr float kSensMax = 0.05f;
constexpr float kGateMin = 0.0005f;
constexpr float kGateMax = 0.02f;

void populateChoiceCombo(juce::ComboBox& combo,
                         juce::AudioProcessorValueTreeState& apvts,
                         const char* paramId)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramId))) {
        combo.clear(juce::dontSendNotification);
        combo.addItemList(choice->choices, 1);
        combo.setSelectedItemIndex(choice->getIndex(), juce::dontSendNotification);
    }
}
} // namespace

GuitarTransducerAudioProcessorEditor::GuitarTransducerAudioProcessorEditor(
    GuitarTransducerAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor_(p),
      telemetry_meter_(p.telemetry())
{
    setLookAndFeel(&look_and_feel_);
    setOpaque(true);

    // Header factory preset selector -> GTRJ v1 state serialization
    for (size_t i = 0; i < transducer::presets::kNumFactoryPresets; ++i) {
        preset_combo_.addItem(transducer::presets::kFactoryPresets[i].name,
                              static_cast<int>(i) + 1);
    }
    preset_combo_.setSelectedItemIndex(processor_.getCurrentProgram(), juce::dontSendNotification);
    preset_combo_.setLookAndFeel(&look_and_feel_);
    preset_combo_.onChange = [this] {
        const int idx = preset_combo_.getSelectedItemIndex();
        if (idx < 0 || idx >= static_cast<int>(transducer::presets::kNumFactoryPresets))
            return;
        const auto& pr = transducer::presets::kFactoryPresets[static_cast<size_t>(idx)];

        juce::ValueTree state(processor_.apvts().state.getType());
        state.setProperty("magic", "GTRJ", nullptr);
        state.setProperty("version", GuitarTransducerAudioProcessor::kStateVersion, nullptr);
        auto addParam = [&](const char* id, float val) {
            juce::ValueTree pNode("PARAM");
            pNode.setProperty("id", id, nullptr);
            pNode.setProperty("value", val, nullptr);
            state.appendChild(pNode, nullptr);
        };
        addParam("bend_range", pr.bend_range);
        addParam("transient_sensitivity", pr.transient_sensitivity);
        addParam("gate_threshold", pr.gate_threshold);
        addParam("routing_mode", static_cast<float>(pr.routing_mode));
        addParam("output_dialect", static_cast<float>(pr.output_dialect));

        if (auto xml = state.createXml()) {
            juce::MemoryBlock block;
            juce::AudioProcessor::copyXmlToBinary(*xml, block);
            processor_.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
        }
    };
    addAndMakeVisible(preset_combo_);

    // Telemetry strip (PITCH S3, E3 (52), BEND, PRESSURE, S1..S6)
    addAndMakeVisible(telemetry_meter_);

    // Hero Knobs: SENSITIVITY, GATE, BEND RANGE
    auto sensToText = [](double v) {
        const double norm = std::clamp((v - kSensMin) / (kSensMax - kSensMin), 0.0, 1.0);
        return juce::String(norm, 2);
    };
    auto sensFromText = [](const juce::String& text) {
        const double norm = std::clamp(text.getDoubleValue(), 0.0, 1.0);
        return kSensMin + norm * (kSensMax - kSensMin);
    };

    auto gateToText = [](double v) {
        const double db = juce::Decibels::gainToDecibels(std::max(v, 1e-6), -96.0);
        return juce::String(db, 1) + " dB";
    };
    auto gateFromText = [](const juce::String& text) {
        const double db = text.retainCharacters("-0123456789.").getDoubleValue();
        return std::clamp(static_cast<double>(juce::Decibels::decibelsToGain(db)),
                          static_cast<double>(kGateMin), static_cast<double>(kGateMax));
    };

    auto bendToText = [](double v) {
        return juce::String(static_cast<int>(std::round(v))) + " st";
    };
    auto bendFromText = [](const juce::String& text) {
        return std::clamp(text.retainCharacters("0123456789.").getDoubleValue(), 1.0, 96.0);
    };

    configureRotarySlider(sensitivity_slider_, "orbital", sensToText, sensFromText);
    configureRotarySlider(gate_slider_, "orbital_dense", gateToText, gateFromText);
    configureRotarySlider(bend_range_slider_, "ticks", bendToText, bendFromText);

    sensitivity_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts(), "transient_sensitivity", sensitivity_slider_);
    gate_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts(), "gate_threshold", gate_slider_);
    bend_range_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor_.apvts(), "bend_range", bend_range_slider_);

    // Re-apply custom musical readout formatters after SliderAttachment (which overrides them in its ctor)
    sensitivity_slider_.textFromValueFunction = sensToText;
    sensitivity_slider_.valueFromTextFunction = sensFromText;
    sensitivity_slider_.updateText();

    gate_slider_.textFromValueFunction = gateToText;
    gate_slider_.valueFromTextFunction = gateFromText;
    gate_slider_.updateText();

    bend_range_slider_.textFromValueFunction = bendToText;
    bend_range_slider_.valueFromTextFunction = bendFromText;
    bend_range_slider_.updateText();

    // Routing & Output ComboBoxes
    populateChoiceCombo(routing_combo_, processor_.apvts(), "routing_mode");
    populateChoiceCombo(output_combo_, processor_.apvts(), "output_dialect");
    routing_combo_.setLookAndFeel(&look_and_feel_);
    output_combo_.setLookAndFeel(&look_and_feel_);
    addAndMakeVisible(routing_combo_);
    addAndMakeVisible(output_combo_);

    routing_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), "routing_mode", routing_combo_);
    output_attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor_.apvts(), "output_dialect", output_combo_);

    // Outer 9-slice volcanic bronze chassis overlay (always on top, clicks pass through)
    addAndMakeVisible(frame_overlay_);
    frame_overlay_.toFront(false);

    // 16:9 aspect-matched logical canvas (renders 2048x1152 at 2x DPI)
    setSize(1024, 576);
}

GuitarTransducerAudioProcessorEditor::~GuitarTransducerAudioProcessorEditor()
{
    preset_combo_.setLookAndFeel(nullptr);
    sensitivity_slider_.setLookAndFeel(nullptr);
    gate_slider_.setLookAndFeel(nullptr);
    bend_range_slider_.setLookAndFeel(nullptr);
    routing_combo_.setLookAndFeel(nullptr);
    output_combo_.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

void GuitarTransducerAudioProcessorEditor::configureRotarySlider(
    juce::Slider& slider,
    const juce::String& knobStyle,
    std::function<juce::String(double)> textFromValue,
    std::function<double(const juce::String&)> valueFromText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f,
                               true);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 22);
    slider.getProperties().set("knobStyle", knobStyle);
    slider.textFromValueFunction = std::move(textFromValue);
    slider.valueFromTextFunction = std::move(valueFromText);
    slider.setLookAndFeel(&look_and_feel_);
    addAndMakeVisible(slider);
}

void GuitarTransducerAudioProcessorEditor::resized()
{
    frame_overlay_.setBounds(getLocalBounds());
    frame_overlay_.toFront(false);

    // Safe interior canvas nested cleanly inside the 9-sliced outer chassis opening
    auto content = getLocalBounds().reduced(64, 47);
    content.removeFromBottom(4); // Balance bottom chassis lip

    // 1. Header row (GUITAR MPE TRANSDUCER | preset ComboBox | CLAP / MPE / MIDI 2.0)
    auto headerRow = content.removeFromTop(32);
    badge_bounds_ = headerRow.removeFromRight(142);
    headerRow.removeFromRight(12);
    auto presetArea = headerRow.removeFromRight(218);
    preset_combo_.setBounds(presetArea.withSizeKeepingCentre(presetArea.getWidth(), 28));
    headerRow.removeFromRight(16);
    title_bounds_ = headerRow;

    content.removeFromTop(4);

    // 2. Live telemetry + S1..S6 string strip
    auto telemetryRow = content.removeFromTop(72);
    telemetry_meter_.setBounds(telemetryRow);

    content.removeFromTop(10);

    // 3. Bottom ROUTING & OUTPUT panel (reserve cleanly with generous breathing room)
    routing_panel_bounds_ = content.removeFromBottom(72);
    content.removeFromBottom(10);

    auto routingInner = routing_panel_bounds_.reduced(12, 6);
    routing_header_bounds_ = routingInner.removeFromTop(18);
    routingInner.removeFromTop(5);
    auto routingControlsRow = routingInner.removeFromTop(30);

    auto leftRoute = routingControlsRow.removeFromLeft(routingControlsRow.getWidth() / 2 - 10);
    routingControlsRow.removeFromLeft(20);
    auto rightRoute = routingControlsRow;

    routing_label_bounds_ = leftRoute.removeFromLeft(66);
    leftRoute.removeFromLeft(6);
    routing_combo_.setBounds(leftRoute.withSizeKeepingCentre(leftRoute.getWidth(), 29));

    output_label_bounds_ = rightRoute.removeFromLeft(58);
    rightRoute.removeFromLeft(6);
    output_combo_.setBounds(rightRoute.withSizeKeepingCentre(rightRoute.getWidth(), 29));

    // 4. Center hero DETECTION & TRACKING panel
    detection_panel_bounds_ = content;
    auto detCopy = detection_panel_bounds_;
    detection_header_bounds_ = detCopy.removeFromTop(26);
    detection_body_bounds_ = detCopy;

    auto knobsBay = detection_body_bounds_.reduced(16, 6);
    const int colW = knobsBay.getWidth() / 3;
    juce::Slider* sliders[3] = {&sensitivity_slider_, &gate_slider_, &bend_range_slider_};

    for (int i = 0; i < 3; ++i) {
        auto col = knobsBay.withX(knobsBay.getX() + i * colW).withWidth(colW);
        knob_label_bounds_[i] = col.removeFromTop(18);
        sliders[i]->setBounds(col);
    }
}

void GuitarTransducerAudioProcessorEditor::paint(juce::Graphics& g)
{
    const auto& assets = transducer::ui::UiAssets::get();
    const auto fullBounds = getLocalBounds().toFloat();

    // 1. Base obsidian background with subtle tiled volcanic rock texture inside the chassis
    g.fillAll(juce::Colour(0xFF0A0806));
    if (assets.lavaRockTile.isValid()) {
        juce::Graphics::ScopedSaveState state(g);
        const auto innerWell = fullBounds.reduced(48.0f, 40.0f);
        g.reduceClipRegion(innerWell.toNearestInt());
        g.setTiledImageFill(assets.lavaRockTile, 0, 0, 0.72f);
        g.fillRect(innerWell);

        // Darken upper header & lower routing zones subtly so telemetry & title stay ultra-legible
        g.setColour(juce::Colour(0x9A0A0806));
        g.fillRect(innerWell);

        juce::ColourGradient outerVig(juce::Colours::transparentBlack,
                                      innerWell.getCentreX(), innerWell.getCentreY(),
                                      juce::Colour(0x99050403),
                                      innerWell.getX(), innerWell.getY(), true);
        g.setGradientFill(outerVig);
        g.fillRect(innerWell);
    }

    // 2. Main Title ("GUITAR MPE TRANSDUCER") — bold warm amber-orange gradient + 2-pass outer glow
    {
        const auto titleFont = transducer::ui::makeUiFont(29.5f, true, true);
        transducer::ui::drawGlowingAmberText(
            g, "GUITAR MPE TRANSDUCER", titleFont,
            title_bounds_.toFloat(), juce::Justification::centredLeft,
            juce::Colour(0xFFFFDE8A), juce::Colour(0xFFFF6B1A), juce::Colour(0xFFFF6B1A));
    }

    // 3. Header Format Badge ("CLAP / MPE / MIDI 2.0")
    {
        g.setFont(transducer::ui::makeUiFont(12.5f, false, true));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText("CLAP / MPE / MIDI 2.0", badge_bounds_.translated(0, 1), juce::Justification::centredRight, false);
        g.setColour(TransducerLookAndFeel::kBronzeText);
        g.drawText("CLAP / MPE / MIDI 2.0", badge_bounds_, juce::Justification::centredRight, false);
    }

    // 4. DETECTION & TRACKING sub-panel (weathered bronze header strip + slim beveled oxidized bronze border + tiled lava rock interior)
    {
        const auto panelF = detection_panel_bounds_.toFloat();
        const auto headerF = detection_header_bounds_.toFloat();
        const auto bodyF = detection_body_bounds_.toFloat();

        // Outer grounded shadow
        g.setColour(juce::Colours::black.withAlpha(0.72f));
        g.fillRoundedRectangle(panelF.expanded(2.0f).translated(0.0f, 2.0f), 6.0f);

        // Clip interior to rounded panel bounds
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path panelClip;
            panelClip.addRoundedRectangle(panelF.reduced(1.5f), 5.0f);
            g.reduceClipRegion(panelClip);

            // 4a. Interior bay filled with tiled volcanic lava-rock texture + subtle dark radial vignette
            if (assets.lavaRockTile.isValid()) {
                g.setTiledImageFill(assets.lavaRockTile,
                                    detection_body_bounds_.getX(),
                                    detection_body_bounds_.getY(),
                                    1.0f);
                g.fillRect(bodyF);
            } else {
                g.setColour(juce::Colour(0xFF14100C));
                g.fillRect(bodyF);
            }

            // Subtle dark radial vignette keeping volcanic basalt rock clearly textured behind knobs
            juce::ColourGradient vignette(juce::Colour(0x04000000),
                                          bodyF.getCentreX(), bodyF.getCentreY(),
                                          juce::Colour(0x78060403),
                                          bodyF.getX(), bodyF.getY(), true);
            g.setGradientFill(vignette);
            g.fillRect(bodyF);

            // Subtle inner recessed drop shadow along the bay perimeter
            juce::ColourGradient topShadow(juce::Colour(0xAA000000), bodyF.getX(), bodyF.getY(),
                                           juce::Colours::transparentBlack, bodyF.getX(), bodyF.getY() + 14.0f, false);
            g.setGradientFill(topShadow);
            g.fillRect(bodyF.getX(), bodyF.getY(), bodyF.getWidth(), 14.0f);

            juce::ColourGradient botShadow(juce::Colours::transparentBlack, bodyF.getX(), bodyF.getBottom() - 14.0f,
                                           juce::Colour(0x99000000), bodyF.getX(), bodyF.getBottom(), false);
            g.setGradientFill(botShadow);
            g.fillRect(bodyF.getX(), bodyF.getBottom() - 14.0f, bodyF.getWidth(), 14.0f);

            // 4b. Weathered oxidized bronze header strip (~26px tall) across top
            if (assets.bronzeHeaderStrip.isValid()) {
                g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                g.drawImage(assets.bronzeHeaderStrip, headerF, juce::RectanglePlacement::stretchToFit);
            } else {
                juce::ColourGradient hdrGrad(juce::Colour(0xFF6B4A2F), headerF.getX(), headerF.getY(),
                                             juce::Colour(0xFF3E2918), headerF.getX(), headerF.getBottom(), false);
                g.setGradientFill(hdrGrad);
                g.fillRect(headerF);
            }

            // Subtle horizontal bronze metallic sheen + crisp divider line below header strip
            juce::ColourGradient hdrSheen(juce::Colour(0x28FFD8B0), headerF.getX(), headerF.getY(),
                                          juce::Colour(0x440A0604), headerF.getX(), headerF.getBottom(), false);
            g.setGradientFill(hdrSheen);
            g.fillRect(headerF);

            g.setColour(juce::Colour(0xAA0A0705));
            g.drawHorizontalLine(static_cast<int>(headerF.getBottom()) - 1, headerF.getX(), headerF.getRight());
            g.setColour(TransducerLookAndFeel::kBronzeLight.withAlpha(0.55f));
            g.drawHorizontalLine(static_cast<int>(headerF.getY()) + 2, headerF.getX() + 3.0f, headerF.getRight() - 3.0f);
        }

        // 4c. Slim (3.5px) beveled oxidized bronze border (#6B4A2F to #8B5A3C) — no lava seams or giant rivets
        {
            juce::ColourGradient outerBevel(TransducerLookAndFeel::kBronzeLight, panelF.getX(), panelF.getY(),
                                            TransducerLookAndFeel::kBronzeBorder.darker(0.35f), panelF.getX(), panelF.getBottom(), false);
            g.setGradientFill(outerBevel);
            g.drawRoundedRectangle(panelF.reduced(1.5f), 5.0f, 3.2f);

            // Inner dark crisp rim + outer specular hairline
            g.setColour(juce::Colour(0xCC0A0705));
            g.drawRoundedRectangle(panelF.reduced(3.2f), 3.8f, 1.0f);
            g.setColour(juce::Colour(0x44E0B088));
            g.drawRoundedRectangle(panelF.reduced(0.5f), 5.5f, 0.8f);
        }

        // 4d. Left-aligned "DETECTION & TRACKING" label inside bronze header strip
        {
            auto hdrText = detection_header_bounds_.reduced(12, 0);
            g.setFont(transducer::ui::makeUiFont(13.5f, true, true));
            g.setColour(juce::Colours::black.withAlpha(0.80f));
            g.drawText("DETECTION & TRACKING", hdrText.translated(0, 1), juce::Justification::centredLeft, false);
            g.setColour(TransducerLookAndFeel::kCreamText);
            g.drawText("DETECTION & TRACKING", hdrText, juce::Justification::centredLeft, false);
        }

        // 4e. Hero Knob Labels ("SENSITIVITY", "GATE", "BEND RANGE")
        static constexpr const char* kKnobTitles[3] = {"SENSITIVITY", "GATE", "BEND RANGE"};
        g.setFont(transducer::ui::makeUiFont(13.0f, false, true));
        for (int i = 0; i < 3; ++i) {
            g.setColour(juce::Colours::black.withAlpha(0.85f));
            g.drawText(kKnobTitles[i], knob_label_bounds_[i].translated(0, 1), juce::Justification::centred, false);
            g.setColour(TransducerLookAndFeel::kBronzeText);
            g.drawText(kKnobTitles[i], knob_label_bounds_[i], juce::Justification::centred, false);
        }
    }

    // 5. ROUTING & OUTPUT sub-panel (slim 2.5px recessed bronze outline matching inner panel language)
    {
        const auto rf = routing_panel_bounds_.toFloat();

        // Dark recessed translucent rock backing
        g.setColour(juce::Colour(0xB80E0A08));
        g.fillRoundedRectangle(rf.reduced(1.0f), 4.5f);

        // Subtle top inner drop shadow
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clip;
            clip.addRoundedRectangle(rf.reduced(1.5f), 4.0f);
            g.reduceClipRegion(clip);
            juce::ColourGradient topShad(juce::Colour(0x88000000), rf.getX(), rf.getY(),
                                         juce::Colours::transparentBlack, rf.getX(), rf.getY() + 10.0f, false);
            g.setGradientFill(topShad);
            g.fillRect(rf.getX(), rf.getY(), rf.getWidth(), 10.0f);
        }

        // Slim 2.5px beveled oxidized bronze outline
        juce::ColourGradient routeBorder(TransducerLookAndFeel::kBronzeLight.withAlpha(0.85f), rf.getX(), rf.getY(),
                                         TransducerLookAndFeel::kBronzeBorder.darker(0.30f), rf.getX(), rf.getBottom(), false);
        g.setGradientFill(routeBorder);
        g.drawRoundedRectangle(rf.reduced(1.25f), 4.5f, 2.4f);

        g.setColour(juce::Colour(0xAA080604));
        g.drawRoundedRectangle(rf.reduced(2.6f), 3.6f, 0.9f);

        // Panel header & field labels
        g.setFont(transducer::ui::makeUiFont(13.0f, true, true));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText("ROUTING & OUTPUT", routing_header_bounds_.translated(0, 1), juce::Justification::centredLeft, false);
        g.setColour(TransducerLookAndFeel::kBronzeText);
        g.drawText("ROUTING & OUTPUT", routing_header_bounds_, juce::Justification::centredLeft, false);

        g.setFont(transducer::ui::makeUiFont(13.0f, false, true));
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawText("ROUTING:", routing_label_bounds_.translated(0, 1), juce::Justification::centredLeft, false);
        g.drawText("OUTPUT:", output_label_bounds_.translated(0, 1), juce::Justification::centredLeft, false);
        g.setColour(TransducerLookAndFeel::kBronzeText);
        g.drawText("ROUTING:", routing_label_bounds_, juce::Justification::centredLeft, false);
        g.drawText("OUTPUT:", output_label_bounds_, juce::Justification::centredLeft, false);
    }
}
