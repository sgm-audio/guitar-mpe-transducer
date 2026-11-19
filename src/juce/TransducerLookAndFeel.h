#pragma once

#include "UiSliceHelpers.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

class TransducerLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    static inline const juce::Colour kGrooveDark{0xFF0A0806};
    static inline const juce::Colour kTabDark{0xFF14100C};
    static inline const juce::Colour kBronzeBorder{0xFF6B4A2F};
    static inline const juce::Colour kBronzeLight{0xFF8B5A3C};
    static inline const juce::Colour kCreamText{0xFFF5E6D3};
    static inline const juce::Colour kBronzeText{0xFFC4A882};
    static inline const juce::Colour kAmberGlow{0xFFFF9A3C};
    static inline const juce::Colour kAmberBright{0xFFFFD080};
    static inline const juce::Colour kMoltenOrange{0xFFFF6B1A};

    TransducerLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, kMoltenOrange);
        setColour(juce::Slider::rotarySliderOutlineColourId, kBronzeBorder);
        setColour(juce::Slider::thumbColourId, kAmberGlow);
        setColour(juce::Slider::textBoxTextColourId, kAmberGlow);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xFF110D0A));
        setColour(juce::Slider::textBoxOutlineColourId, kBronzeBorder);

        setColour(juce::ComboBox::backgroundColourId, kGrooveDark);
        setColour(juce::ComboBox::textColourId, kCreamText);
        setColour(juce::ComboBox::outlineColourId, kBronzeBorder);
        setColour(juce::ComboBox::arrowColourId, kAmberGlow);

        setColour(juce::Label::textColourId, kCreamText);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        setColour(juce::Label::outlineColourId, juce::Colours::transparentBlack);

        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xFF16110D));
        setColour(juce::PopupMenu::textColourId, kCreamText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kMoltenOrange.withAlpha(0.28f));
        setColour(juce::PopupMenu::highlightedTextColourId, kAmberBright);
    }

    juce::Slider::SliderLayout getSliderLayout(juce::Slider& slider) override
    {
        juce::Slider::SliderLayout layout;
        auto local = slider.getLocalBounds();

        const int textH = 21;
        // Compact low-profile readout badge tucked directly beneath each knob's orbital ring
        const juce::String txt = slider.getTextFromValue(slider.getValue());
        const int textW = std::clamp(static_cast<int>(txt.length()) * 9 + 20, 52, 72);
        const int gap = 2;

        auto textArea = local.removeFromBottom(textH);
        layout.textBoxBounds = textArea.withSizeKeepingCentre(textW, textH);
        local.removeFromBottom(gap);
        layout.sliderBounds = local;
        return layout;
    }

    juce::Label* createSliderTextBox(juce::Slider& slider) override
    {
        auto* l = juce::LookAndFeel_V4::createSliderTextBox(slider);
        l->setJustificationType(juce::Justification::centred);
        l->setFont(transducer::ui::makeUiFont(13.0f, true, false));
        l->setColour(juce::Label::textColourId, kAmberGlow);
        l->setColour(juce::Label::backgroundColourId, juce::Colour(0xFF110D0A));
        l->setColour(juce::Label::outlineColourId, kBronzeBorder);
        l->setBorderSize(juce::BorderSize<int>(1, 3, 1, 3));
        l->getProperties().set("isKnobReadout", true);
        return l;
    }

    void drawLabel(juce::Graphics& g, juce::Label& label) override
    {
        if (label.getProperties().getWithDefault("isKnobReadout", false)) {
            const auto b = label.getLocalBounds().toFloat().reduced(0.5f);

            // Low-profile recessed oxidized bronze badge
            g.setColour(juce::Colours::black.withAlpha(0.55f));
            g.fillRoundedRectangle(b.translated(0.0f, 1.0f), 3.0f);

            juce::ColourGradient fillGrad(juce::Colour(0xE80A0705), b.getX(), b.getY(),
                                          juce::Colour(0xE818100A), b.getX(), b.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(b, 3.0f);

            // Subtle warm inner amber glow behind readout text
            juce::ColourGradient glow(kMoltenOrange.withAlpha(0.14f), b.getCentreX(), b.getCentreY(),
                                      juce::Colours::transparentBlack, b.getX(), b.getCentreY(), true);
            g.setGradientFill(glow);
            g.fillRoundedRectangle(b.reduced(1.0f), 2.5f);

            // Beveled bronze outline
            juce::ColourGradient borderGrad(kBronzeLight.withAlpha(0.68f), b.getX(), b.getY(),
                                            kBronzeBorder.darker(0.40f), b.getX(), b.getBottom(), false);
            g.setGradientFill(borderGrad);
            g.drawRoundedRectangle(b, 3.0f, 1.0f);

            // Warm glowing amber readout text (#FF9A3C / #F5E6D3)
            const auto font = transducer::ui::makeUiFont(13.0f, true, false);
            g.setFont(font);
            g.setColour(juce::Colours::black.withAlpha(0.75f));
            g.drawText(label.getText(), label.getLocalBounds().translated(0, 1),
                       juce::Justification::centred, false);
            g.setColour(kAmberGlow);
            g.drawText(label.getText(), label.getLocalBounds(),
                       juce::Justification::centred, false);
            return;
        }

        // Standard label (including ComboBox interior text label)
        if (!label.isBeingEdited()) {
            const auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
            g.setFont(getLabelFont(label));
            const auto col = label.findColour(juce::Label::textColourId)
                                 .withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.5f);
            g.setColour(juce::Colours::black.withAlpha(0.65f));
            g.drawFittedText(label.getText(), textArea.translated(0, 1),
                             label.getJustificationType(), 1, 0.92f);
            g.setColour(col);
            g.drawFittedText(label.getText(), textArea,
                             label.getJustificationType(), 1, 0.92f);
        }
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getProperties().getWithDefault("isKnobReadout", false))
            return transducer::ui::makeUiFont(13.0f, true, false);
        return label.getFont();
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float maxR = std::max(24.0f, static_cast<float>(std::min(width, height)) * 0.5f - 2.0f);
        const float knobR = maxR * 0.575f;
        const float arcR = knobR + std::max(4.5f, maxR * 0.068f);
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        const juce::String style = slider.getProperties().getWithDefault("knobStyle", "orbital").toString();

        // 1. Crisp programmatic vector scale rings / orbital geometry (behind knob cap)
        if (style == "ticks") {
            drawBendRangeVectorScale(g, cx, cy, knobR, arcR, maxR, rotaryStartAngle, rotaryEndAngle, angle, sliderPos);
        } else {
            const bool dense = (style == "orbital_dense");
            drawOrbitalVectorScale(g, cx, cy, knobR, arcR, maxR, rotaryStartAngle, rotaryEndAngle, angle, sliderPos, dense);
        }

        // 2. Active glowing value arc hugging the knob perimeter
        if (sliderPos > 0.002f) {
            juce::Path activeArc;
            activeArc.addCentredArc(cx, cy, arcR, arcR, 0.0f, rotaryStartAngle, angle, true);

            // Outer warm bloom
            g.setColour(kMoltenOrange.withAlpha(0.18f));
            g.strokePath(activeArc, juce::PathStrokeType(7.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(kMoltenOrange.withAlpha(0.42f));
            g.strokePath(activeArc, juce::PathStrokeType(4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Core incandescent stroke
            juce::ColourGradient arcGrad(kMoltenOrange, cx - arcR, cy + arcR,
                                         kAmberBright, cx + arcR, cy - arcR, false);
            g.setGradientFill(arcGrad);
            g.strokePath(activeArc, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Knob grounded drop shadow & recessed dark collar (unrotated)
        {
            juce::ColourGradient shadowGrad(juce::Colours::black.withAlpha(0.88f), cx, cy + knobR * 0.14f,
                                            juce::Colours::transparentBlack, cx, cy + knobR * 1.28f, true);
            g.setGradientFill(shadowGrad);
            g.fillEllipse(cx - knobR * 1.18f, cy - knobR * 1.04f, knobR * 2.36f, knobR * 2.36f);

            // Dark recessed metallic well collar around cap
            g.setColour(juce::Colour(0xFF090706));
            g.fillEllipse(cx - knobR - 2.5f, cy - knobR - 2.5f, (knobR + 2.5f) * 2.0f, (knobR + 2.5f) * 2.0f);
            g.setColour(kBronzeBorder.withAlpha(0.45f));
            g.drawEllipse(cx - knobR - 1.5f, cy - knobR - 1.5f, (knobR + 1.5f) * 2.0f, (knobR + 1.5f) * 2.0f, 1.0f);
        }

        // 4. Isolated circular-masked dark volcanic rock knob cap rotated around exact center
        const auto& knobImg = transducer::ui::UiAssets::get().knobCap;
        {
            juce::Graphics::ScopedSaveState state(g);
            juce::Path clipCircle;
            clipCircle.addEllipse(cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
            g.reduceClipRegion(clipCircle);

            if (knobImg.isValid()) {
                const float imgW = static_cast<float>(knobImg.getWidth());
                const float imgH = static_cast<float>(knobImg.getHeight());
                // knob_cap.png has its circular rock content at radius 246/512 of image size
                const float scale = (knobR * 2.0f) / (imgW * (492.0f / 512.0f));
                auto xf = juce::AffineTransform::translation(-imgW * 0.5f, -imgH * 0.5f)
                              .scaled(scale)
                              .rotated(angle)
                              .translated(cx, cy);
                g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
                g.drawImageTransformed(knobImg, xf, false);
            } else {
                g.setColour(juce::Colour(0xFF1D1A18));
                g.fillEllipse(cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
            }

            // Subtle unrotated top-left bevel highlight & bottom-right depth shading
            juce::ColourGradient bevelShade(juce::Colour(0x28FFD8B0), cx - knobR * 0.65f, cy - knobR * 0.75f,
                                            juce::Colour(0x55000000), cx + knobR * 0.75f, cy + knobR * 0.80f, false);
            g.setGradientFill(bevelShade);
            g.fillEllipse(cx - knobR, cy - knobR, knobR * 2.0f, knobR * 2.0f);
        }

        // Crisp bevel ring around the dark volcanic cap
        {
            juce::ColourGradient rimGrad(juce::Colour(0x66B08A68), cx - knobR * 0.5f, cy - knobR,
                                         juce::Colour(0xAA050403), cx + knobR * 0.5f, cy + knobR, false);
            g.setGradientFill(rimGrad);
            g.drawEllipse(cx - knobR + 0.5f, cy - knobR + 0.5f, (knobR - 0.5f) * 2.0f, (knobR - 0.5f) * 2.0f, 1.35f);
        }

        // 5. Glowing #FF6B1A pointer slit rotated around exact (cx, cy)
        {
            const float slitLen = knobR * 0.36f;
            const float slitW = std::max(3.4f, knobR * 0.068f);
            const float slitTop = -knobR + knobR * 0.12f;
            auto rot = juce::AffineTransform::rotation(angle).translated(cx, cy);

            // Recessed dark groove
            juce::Path groove;
            groove.addRoundedRectangle(-slitW * 0.78f, slitTop - 1.5f, slitW * 1.56f, slitLen + 3.0f, slitW * 0.78f);
            g.setColour(juce::Colours::black.withAlpha(0.82f));
            g.fillPath(groove, rot);

            // Outer fiery bloom
            juce::Path bloomOuter;
            bloomOuter.addRoundedRectangle(-slitW * 1.65f, slitTop - 4.0f, slitW * 3.3f, slitLen + 8.0f, slitW * 1.65f);
            g.setColour(kMoltenOrange.withAlpha(0.20f));
            g.fillPath(bloomOuter, rot);

            juce::Path bloomMid;
            bloomMid.addRoundedRectangle(-slitW * 1.05f, slitTop - 1.5f, slitW * 2.1f, slitLen + 3.0f, slitW * 1.05f);
            g.setColour(kMoltenOrange.withAlpha(0.55f));
            g.fillPath(bloomMid, rot);

            // Bright orange slit body
            juce::Path slit;
            slit.addRoundedRectangle(-slitW * 0.5f, slitTop, slitW, slitLen, slitW * 0.5f);
            g.setColour(kMoltenOrange);
            g.fillPath(slit, rot);

            // Incandescent amber-white hot core
            juce::Path core;
            core.addRoundedRectangle(-slitW * 0.28f, slitTop + 1.2f, slitW * 0.56f, slitLen - 2.4f, slitW * 0.28f);
            g.setColour(kAmberBright);
            g.fillPath(core, rot);
        }
    }

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int, int, int, int,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                                   static_cast<float>(width),
                                                   static_cast<float>(height));
        const auto& barImg = transducer::ui::UiAssets::get().comboBoxBar;

        if (barImg.isValid()) {
            // Horizontal 3-slice: left bezel (72px) and right arrow housing + divider (172px)
            // preserve exact 1:1 aspect ratio at any ComboBox width.
            transducer::ui::drawThreeSliceHorizontal(g, barImg, bounds, 72, 172);
        } else {
            g.setColour(kGrooveDark);
            g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);
            g.setColour(kBronzeBorder);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.5f);
        }

        if (isButtonDown || box.hasKeyboardFocus(false)) {
            g.setColour(kAmberGlow.withAlpha(0.45f));
            g.drawRoundedRectangle(bounds.reduced(0.75f), 3.0f, 1.25f);
        }
    }

    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        const int leftInset = 14;
        const int rightInset = box.getHeight() + 8;
        const int textW = std::max(1, box.getWidth() - leftInset - rightInset);
        const int textH = std::max(1, box.getHeight() - 2);

        label.setBounds(leftInset, 1, textW, textH);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setBorderSize(juce::BorderSize<int>(0, 0, 0, 0));
        label.setColour(juce::Label::textColourId, juce::Colour(0xFFF2CFA0));
        label.setFont(transducer::ui::makeUiFont(
            std::clamp(static_cast<float>(box.getHeight()) * 0.48f, 11.5f, 14.5f), false, false));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        return transducer::ui::makeUiFont(
            std::clamp(static_cast<float>(box.getHeight()) * 0.48f, 11.5f, 14.5f), false, false);
    }

private:
    static void drawOrbitalVectorScale(juce::Graphics& g,
                                       float cx, float cy,
                                       float knobR, float arcR, float maxR,
                                       float startAngle, float endAngle,
                                       float /*angle*/, float /*sliderPos*/,
                                       bool dense)
    {
        const float r1 = arcR;
        const float r2 = arcR + (maxR - arcR) * 0.24f;
        const float r3 = arcR + (maxR - arcR) * 0.50f;
        const float r4 = arcR + (maxR - arcR) * 0.74f;
        const float r5 = maxR * 0.94f;

        // Subtle ambient orange halo behind orbital rings
        {
            juce::ColourGradient halo(kMoltenOrange.withAlpha(0.12f), cx, cy,
                                      juce::Colours::transparentBlack, cx + maxR, cy, true);
            g.setGradientFill(halo);
            g.fillEllipse(cx - maxR, cy - maxR, maxR * 2.0f, maxR * 2.0f);
        }

        // Layered thin concentric circles
        auto strokeCircle = [&](float r, float alpha, float thickness) {
            g.setColour(kAmberGlow.withAlpha(alpha));
            g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, thickness);
        };
        strokeCircle(r1, 0.28f, 1.0f);
        strokeCircle(r2, 0.36f, 0.9f);
        strokeCircle(r3, 0.28f, 0.8f);
        strokeCircle(r4, 0.24f, 0.75f);
        strokeCircle(r5, 0.18f, 0.65f);

        // Inactive scale track arc
        {
            juce::Path baseArc;
            baseArc.addCentredArc(cx, cy, r1, r1, 0.0f, startAngle, endAngle, true);
            g.setColour(kBronzeLight.withAlpha(0.32f));
            g.strokePath(baseArc, juce::PathStrokeType(1.6f));
        }

        // Sacred geometric star / inscribed chord polygons tightly inscribed inside outer rings
        auto drawPolygon = [&](int sides, int step, float r, float rotOffset, float alpha, float stroke) {
            juce::Path p;
            for (int i = 0; i < sides; ++i) {
                const float a0 = rotOffset + static_cast<float>(i) * juce::MathConstants<float>::twoPi / static_cast<float>(sides);
                const float a1 = rotOffset + static_cast<float>((i + step) % sides) * juce::MathConstants<float>::twoPi / static_cast<float>(sides);
                p.startNewSubPath(cx + std::cos(a0) * r, cy + std::sin(a0) * r);
                p.lineTo(cx + std::cos(a1) * r, cy + std::sin(a1) * r);
            }
            g.setColour(kAmberGlow.withAlpha(alpha));
            g.strokePath(p, juce::PathStrokeType(stroke));
        };

        // 8-point geometric star chords + outer diamond inscribed strictly inside r5
        drawPolygon(8, 3, r4, -juce::MathConstants<float>::halfPi, dense ? 0.36f : 0.26f, 0.75f);
        drawPolygon(8, 2, r4, -juce::MathConstants<float>::halfPi, dense ? 0.30f : 0.20f, 0.70f);
        drawPolygon(4, 1, r5, -juce::MathConstants<float>::halfPi, dense ? 0.34f : 0.24f, 0.75f);
        if (dense) {
            // Inscribed rotated square at r4 (stays inside r5 circle) + 12-point star
            drawPolygon(4, 1, r4, -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi * 0.25f, 0.28f, 0.70f);
            drawPolygon(12, 5, r3, 0.0f, 0.22f, 0.60f);
        }

        // Fine radial tick marks around inner orbital ring
        {
            juce::Path ticks;
            const int numTicks = 36;
            for (int i = 0; i < numTicks; ++i) {
                const float a = startAngle + (static_cast<float>(i) / static_cast<float>(numTicks - 1)) * (endAngle - startAngle);
                const float sinA = std::sin(a);
                const float cosA = -std::cos(a);
                const bool major = (i % 6 == 0);
                const float inner = r1 + 1.5f;
                const float outer = r1 + (major ? 5.5f : 3.0f);
                ticks.startNewSubPath(cx + sinA * inner, cy + cosA * inner);
                ticks.lineTo(cx + sinA * outer, cy + cosA * outer);
            }
            g.setColour(kAmberGlow.withAlpha(0.34f));
            g.strokePath(ticks, juce::PathStrokeType(0.75f));
        }

        // Cardinal crosshairs (N/S/E/W) with small target reticles
        {
            juce::Path cross;
            const float inner = knobR + 2.0f;
            const float outer = maxR * 1.00f;
            for (int k = 0; k < 4; ++k) {
                const float a = static_cast<float>(k) * juce::MathConstants<float>::halfPi;
                const float dx = std::cos(a);
                const float dy = std::sin(a);
                cross.startNewSubPath(cx + dx * inner, cy + dy * inner);
                cross.lineTo(cx + dx * outer, cy + dy * outer);

                const float dotR = 3.0f;
                const float tx = cx + dx * (r5 + 2.5f);
                const float ty = cy + dy * (r5 + 2.5f);
                cross.addEllipse(tx - dotR, ty - dotR, dotR * 2.0f, dotR * 2.0f);
            }
            g.setColour(kAmberGlow.withAlpha(0.42f));
            g.strokePath(cross, juce::PathStrokeType(0.85f));
        }

        // Tiny ornamental node circles at 45-degree intersections
        for (int k = 0; k < 4; ++k) {
            const float a = juce::MathConstants<float>::pi * 0.25f + static_cast<float>(k) * juce::MathConstants<float>::halfPi;
            const float nx = cx + std::cos(a) * r3;
            const float ny = cy + std::sin(a) * r3;
            g.setColour(kAmberGlow.withAlpha(0.36f));
            g.drawEllipse(nx - 2.4f, ny - 2.4f, 4.8f, 4.8f, 0.8f);
        }
    }

    static void drawBendRangeVectorScale(juce::Graphics& g,
                                         float cx, float cy,
                                         float /*knobR*/, float arcR, float maxR,
                                         float startAngle, float endAngle,
                                         float angle, float /*sliderPos*/)
    {
        // Subtle ambient warm glow behind dial
        {
            juce::ColourGradient halo(kMoltenOrange.withAlpha(0.12f), cx, cy,
                                      juce::Colours::transparentBlack, cx + maxR, cy, true);
            g.setGradientFill(halo);
            g.fillEllipse(cx - maxR, cy - maxR, maxR * 2.0f, maxR * 2.0f);
        }

        // Subtle drafting arc & inner baseline circle
        {
            juce::Path track;
            track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
            g.setColour(kBronzeLight.withAlpha(0.35f));
            g.strokePath(track, juce::PathStrokeType(1.4f));

            juce::Path outerDraft;
            outerDraft.addCentredArc(cx, cy, arcR + 11.0f, arcR + 11.0f, 0.0f, startAngle - 0.15f, endAngle + 0.15f, true);
            g.setColour(kAmberGlow.withAlpha(0.20f));
            g.strokePath(outerDraft, juce::PathStrokeType(0.75f));
        }

        // Crisp radial calibration tick marks (active portion glows brighter amber-gold)
        {
            juce::Path minorInactive, minorActive;
            juce::Path majorInactive, majorActive;
            const int totalTicks = 49;
            for (int i = 0; i < totalTicks; ++i) {
                const float frac = static_cast<float>(i) / static_cast<float>(totalTicks - 1);
                const float a = startAngle + frac * (endAngle - startAngle);
                const float sinA = std::sin(a);
                const float cosA = -std::cos(a);
                const bool isMajor = (i % 6 == 0);
                const bool isTop = (i == totalTicks / 2);
                const bool isActive = (a <= angle + 0.01f);

                const float inner = arcR + 2.5f;
                const float outer = arcR + (isTop ? 17.0f : (isMajor ? 12.0f : 6.0f));
                auto& dst = isMajor ? (isActive ? majorActive : majorInactive)
                                    : (isActive ? minorActive : minorInactive);
                dst.startNewSubPath(cx + sinA * inner, cy + cosA * inner);
                dst.lineTo(cx + sinA * outer, cy + cosA * outer);
            }
            g.setColour(kAmberGlow.withAlpha(0.26f));
            g.strokePath(minorInactive, juce::PathStrokeType(0.8f));
            g.setColour(kAmberGlow.withAlpha(0.50f));
            g.strokePath(majorInactive, juce::PathStrokeType(1.15f));
            g.setColour(kAmberGlow.withAlpha(0.65f));
            g.strokePath(minorActive, juce::PathStrokeType(0.95f));
            g.setColour(kAmberBright.withAlpha(0.90f));
            g.strokePath(majorActive, juce::PathStrokeType(1.35f));
        }

        // Two subtle technical drafting reticles in upper-right and lower-left (matching reference)
        auto drawReticle = [&](float rx, float ry, float rad) {
            g.setColour(kAmberGlow.withAlpha(0.28f));
            g.drawEllipse(rx - rad, ry - rad, rad * 2.0f, rad * 2.0f, 0.75f);
            g.drawEllipse(rx - rad * 0.52f, ry - rad * 0.52f, rad * 1.04f, rad * 1.04f, 0.65f);
            g.drawLine(rx - rad * 1.4f, ry, rx + rad * 1.4f, ry, 0.65f);
            g.drawLine(rx, ry - rad * 1.4f, rx, ry + rad * 1.4f, 0.65f);
        };
        drawReticle(cx + maxR * 0.78f, cy - maxR * 0.66f, 12.0f);
        drawReticle(cx - maxR * 0.80f, cy + maxR * 0.68f, 14.0f);

        // Small caliper arc in upper-left
        {
            const float lx = cx - maxR * 0.78f;
            const float ly = cy - maxR * 0.68f;
            juce::Path cal;
            cal.addCentredArc(lx, ly, 12.0f, 12.0f, 0.0f, -0.6f, 1.9f, true);
            cal.startNewSubPath(lx, ly - 15.0f);
            cal.lineTo(lx, ly + 15.0f);
            cal.startNewSubPath(lx - 12.0f, ly);
            cal.lineTo(lx + 12.0f, ly);
            g.setColour(kAmberGlow.withAlpha(0.25f));
            g.strokePath(cal, juce::PathStrokeType(0.75f));
        }
    }
};
