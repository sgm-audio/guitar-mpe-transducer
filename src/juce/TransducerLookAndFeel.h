#pragma once

#include "UiSliceHelpers.h"
#include <jive_style_sheets/geometry/jive_BorderWidth.h>
#include <jive_style_sheets/jive_style_sheets.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

/**
 * Custom JIVE-compatible LookAndFeel for Guitar MPE Transducer.
 *
 * Inherits from jive::LookAndFeel so JIVE's StyleSheet system, JSON style
 * tokens, and Text/ComboBox components resolve styles normally while custom
 * rotary knobs, 3-slice bronze ComboBoxes, and recessed readout badges render
 * with pixel-accurate volcanic bronze hardware styling.
 */
class TransducerLookAndFeel final : public jive::LookAndFeel {
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
    static inline const juce::Colour kMatteOrangePrimary{0xFFC46A34};
    static inline const juce::Colour kMatteOrangeReadout{0xFFB85E2B};
    static inline const juce::Colour kMatteOrangeSecondary{0xFF9E5228};
    static inline const juce::Colour kActiveTabTop{0xFF8C4218};
    static inline const juce::Colour kActiveTabBottom{0xFF6B2F0E};
    static inline const juce::Colour kActiveTabBorder{0xFFA65223};
    static inline const juce::Colour kActiveTabText{0xFFE2D5C8};

    TransducerLookAndFeel()
    {
        applyTheme(jive::Theme::steel);

        setColour(juce::Slider::rotarySliderFillColourId, kMoltenOrange);
        setColour(juce::Slider::rotarySliderOutlineColourId, kBronzeBorder);
        setColour(juce::Slider::thumbColourId, kAmberGlow);
        setColour(juce::Slider::textBoxTextColourId, kMatteOrangeReadout);
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

        const int textH = 24;
        const juce::String txt = slider.getTextFromValue(slider.getValue());
        const int textW = std::clamp(static_cast<int>(txt.length()) * 10 + 24, 58, 86);
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
        l->setFont(transducer::ui::makeUiFont(14.5f, true, false));
        l->setColour(juce::Label::textColourId, kMatteOrangeReadout);
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
            g.setColour(juce::Colours::black.withAlpha(0.58f));
            g.fillRoundedRectangle(b.translated(0.0f, 1.0f), 3.5f);

            juce::ColourGradient fillGrad(juce::Colour(0xEC0A0705), b.getX(), b.getY(),
                                          juce::Colour(0xEC1A110B), b.getX(), b.getBottom(), false);
            g.setGradientFill(fillGrad);
            g.fillRoundedRectangle(b, 3.5f);

            // Beveled oxidized bronze outline
            juce::ColourGradient borderGrad(kBronzeLight.withAlpha(0.72f), b.getX(), b.getY(),
                                            kBronzeBorder.darker(0.42f), b.getX(), b.getBottom(), false);
            g.setGradientFill(borderGrad);
            g.drawRoundedRectangle(b, 3.5f, 1.05f);

            // Flat matte burnt-orange readout text (no glow, no drop shadow)
            const auto font = transducer::ui::makeUiFont(14.5f, true, false);
            g.setFont(font);
            g.setColour(kMatteOrangeReadout);
            g.drawText(label.getText(), label.getLocalBounds(),
                       juce::Justification::centred, false);
            return;
        }

        // ComboBox interior text label: read JIVE styles while keeping sharp warm drop shadow
        if (auto* parentCombo = dynamic_cast<juce::ComboBox*>(label.getParentComponent())) {
            if (!label.isBeingEdited()) {
                const auto styles = findMostApplicableStyles(*parentCombo);
                const auto fgFill = jive::getForegroundFill(*parentCombo, styles, juce::Colour(0xFFF2CFA0));
                const juce::Colour textCol = fgFill.isColour()
                    ? fgFill.colour.withMultipliedAlpha(label.isEnabled() ? 1.0f : 0.5f)
                    : juce::Colour(0xFFF2CFA0);

                const auto textArea = label.getBorderSize().subtractedFrom(label.getLocalBounds());
                g.setFont(getComboBoxFont(*parentCombo));
                g.setColour(juce::Colours::black.withAlpha(0.70f));
                g.drawFittedText(label.getText(), textArea.translated(0, 1),
                                 juce::Justification::centredLeft, 1, 0.92f);
                g.setColour(textCol);
                g.drawFittedText(label.getText(), textArea,
                                 juce::Justification::centredLeft, 1, 0.92f);
            }
            return;
        }

        // All other labels delegate to JIVE's styled LookAndFeel
        jive::LookAndFeel::drawLabel(g, label);
    }

    juce::Font getLabelFont(juce::Label& label) override
    {
        if (label.getProperties().getWithDefault("isKnobReadout", false))
            return transducer::ui::makeUiFont(14.5f, true, false);
        if (auto* parentCombo = dynamic_cast<juce::ComboBox*>(label.getParentComponent()))
            return getComboBoxFont(*parentCombo);
        return jive::LookAndFeel::getLabelFont(label);
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
        const float maxR = std::max(28.0f, static_cast<float>(std::min(width, height)) * 0.5f - 5.0f);
        const float knobR = maxR * 0.575f;
        const float arcR = knobR + std::max(5.0f, maxR * 0.068f);
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
            g.strokePath(activeArc, juce::PathStrokeType(8.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(kMoltenOrange.withAlpha(0.44f));
            g.strokePath(activeArc, juce::PathStrokeType(4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Core incandescent stroke
            juce::ColourGradient arcGrad(kMoltenOrange, cx - arcR, cy + arcR,
                                         kAmberBright, cx + arcR, cy - arcR, false);
            g.setGradientFill(arcGrad);
            g.strokePath(activeArc, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Knob grounded drop shadow & recessed dark collar (unrotated)
        {
            juce::ColourGradient shadowGrad(juce::Colours::black.withAlpha(0.88f), cx, cy + knobR * 0.14f,
                                            juce::Colours::transparentBlack, cx, cy + knobR * 1.28f, true);
            g.setGradientFill(shadowGrad);
            g.fillEllipse(cx - knobR * 1.18f, cy - knobR * 1.04f, knobR * 2.36f, knobR * 2.36f);

            // Dark recessed metallic well collar around cap
            g.setColour(juce::Colour(0xFF090706));
            g.fillEllipse(cx - knobR - 2.8f, cy - knobR - 2.8f, (knobR + 2.8f) * 2.0f, (knobR + 2.8f) * 2.0f);
            g.setColour(kBronzeBorder.withAlpha(0.45f));
            g.drawEllipse(cx - knobR - 1.6f, cy - knobR - 1.6f, (knobR + 1.6f) * 2.0f, (knobR + 1.6f) * 2.0f, 1.1f);
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
                // knob_cap.png circular volcanic cap has radius 246/512 of image width
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
            g.drawEllipse(cx - knobR + 0.5f, cy - knobR + 0.5f, (knobR - 0.5f) * 2.0f, (knobR - 0.5f) * 2.0f, 1.45f);
        }

        // 5. Glowing #FF6B1A pointer slit rotated around exact (cx, cy)
        {
            const float slitLen = knobR * 0.36f;
            const float slitW = std::max(3.8f, knobR * 0.068f);
            const float slitTop = -knobR + knobR * 0.12f;
            auto rot = juce::AffineTransform::rotation(angle).translated(cx, cy);

            // Recessed dark groove
            juce::Path groove;
            groove.addRoundedRectangle(-slitW * 0.78f, slitTop - 1.6f, slitW * 1.56f, slitLen + 3.2f, slitW * 0.78f);
            g.setColour(juce::Colours::black.withAlpha(0.82f));
            g.fillPath(groove, rot);

            // Outer fiery bloom
            juce::Path bloomOuter;
            bloomOuter.addRoundedRectangle(-slitW * 1.65f, slitTop - 4.2f, slitW * 3.3f, slitLen + 8.4f, slitW * 1.65f);
            g.setColour(kMoltenOrange.withAlpha(0.20f));
            g.fillPath(bloomOuter, rot);

            juce::Path bloomMid;
            bloomMid.addRoundedRectangle(-slitW * 1.05f, slitTop - 1.6f, slitW * 2.1f, slitLen + 3.2f, slitW * 1.05f);
            g.setColour(kMoltenOrange.withAlpha(0.55f));
            g.fillPath(bloomMid, rot);

            // Bright orange slit body
            juce::Path slit;
            slit.addRoundedRectangle(-slitW * 0.5f, slitTop, slitW, slitLen, slitW * 0.5f);
            g.setColour(kMoltenOrange);
            g.fillPath(slit, rot);

            // Incandescent amber-white hot core
            juce::Path core;
            core.addRoundedRectangle(-slitW * 0.28f, slitTop + 1.3f, slitW * 0.56f, slitLen - 2.6f, slitW * 0.28f);
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
        // Respect JIVE style sheet font/border tokens while enforcing crisp bezel insets
        // so label text starts cleanly inside the left bronze cap and stops before the
        // right vertical divider + arrow socket without breaking JIVE layout bounds.
        const auto styles = findMostApplicableStyles(box);
        const auto border = jive::getBorderWidth(box, styles);
        const int leftInset = std::max(15, static_cast<int>(std::ceil(border.getLeft())) + 12);
        const int rightInset = box.getHeight() + 9;
        const int textW = std::max(1, box.getWidth() - leftInset - rightInset);
        const int textH = std::max(1, box.getHeight() - 2);

        label.setBounds(leftInset, 1, textW, textH);
        label.setJustificationType(juce::Justification::centredLeft);
        label.setBorderSize(juce::BorderSize<int>(0, 0, 0, 0));
        label.setColour(juce::Label::textColourId, juce::Colour(0xFFF2CFA0));
        label.setFont(getComboBoxFont(box));
    }

    juce::Font getComboBoxFont(juce::ComboBox& box) override
    {
        const auto styles = findMostApplicableStyles(box);
        const float pt = styles.find<float>("font-size").value_or(
            std::clamp(static_cast<float>(box.getHeight()) * 0.48f, 13.0f, 16.0f));
        return transducer::ui::makeUiFont(pt, false, false);
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
        strokeCircle(r1, 0.28f, 1.05f);
        strokeCircle(r2, 0.36f, 0.95f);
        strokeCircle(r3, 0.28f, 0.85f);
        strokeCircle(r4, 0.24f, 0.80f);
        strokeCircle(r5, 0.18f, 0.70f);

        // Inactive scale track arc
        {
            juce::Path baseArc;
            baseArc.addCentredArc(cx, cy, r1, r1, 0.0f, startAngle, endAngle, true);
            g.setColour(kBronzeLight.withAlpha(0.34f));
            g.strokePath(baseArc, juce::PathStrokeType(1.7f));
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
        drawPolygon(8, 3, r4, -juce::MathConstants<float>::halfPi, dense ? 0.38f : 0.28f, 0.82f);
        drawPolygon(8, 2, r4, -juce::MathConstants<float>::halfPi, dense ? 0.32f : 0.22f, 0.75f);
        drawPolygon(4, 1, r5, -juce::MathConstants<float>::halfPi, dense ? 0.36f : 0.26f, 0.82f);
        if (dense) {
            drawPolygon(4, 1, r4, -juce::MathConstants<float>::halfPi + juce::MathConstants<float>::pi * 0.25f, 0.30f, 0.75f);
            drawPolygon(12, 5, r3, 0.0f, 0.24f, 0.65f);
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
                const float inner = r1 + 1.8f;
                const float outer = r1 + (major ? 6.5f : 3.5f);
                ticks.startNewSubPath(cx + sinA * inner, cy + cosA * inner);
                ticks.lineTo(cx + sinA * outer, cy + cosA * outer);
            }
            g.setColour(kAmberGlow.withAlpha(0.36f));
            g.strokePath(ticks, juce::PathStrokeType(0.85f));
        }

        // Cardinal crosshairs (N/S/E/W) with small target reticles
        {
            juce::Path cross;
            const float inner = knobR + 2.5f;
            const float outer = maxR * 1.00f;
            for (int k = 0; k < 4; ++k) {
                const float a = static_cast<float>(k) * juce::MathConstants<float>::halfPi;
                const float dx = std::cos(a);
                const float dy = std::sin(a);
                cross.startNewSubPath(cx + dx * inner, cy + dy * inner);
                cross.lineTo(cx + dx * outer, cy + dy * outer);

                const float dotR = 3.4f;
                const float tx = cx + dx * (r5 + 2.8f);
                const float ty = cy + dy * (r5 + 2.8f);
                cross.addEllipse(tx - dotR, ty - dotR, dotR * 2.0f, dotR * 2.0f);
            }
            g.setColour(kAmberGlow.withAlpha(0.44f));
            g.strokePath(cross, juce::PathStrokeType(0.95f));
        }

        // Tiny ornamental node circles at 45-degree intersections
        for (int k = 0; k < 4; ++k) {
            const float a = juce::MathConstants<float>::pi * 0.25f + static_cast<float>(k) * juce::MathConstants<float>::halfPi;
            const float nx = cx + std::cos(a) * r3;
            const float ny = cy + std::sin(a) * r3;
            g.setColour(kAmberGlow.withAlpha(0.38f));
            g.drawEllipse(nx - 2.8f, ny - 2.8f, 5.6f, 5.6f, 0.85f);
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
            g.setColour(kBronzeLight.withAlpha(0.36f));
            g.strokePath(track, juce::PathStrokeType(1.55f));

            juce::Path outerDraft;
            outerDraft.addCentredArc(cx, cy, arcR + 13.0f, arcR + 13.0f, 0.0f, startAngle - 0.15f, endAngle + 0.15f, true);
            g.setColour(kAmberGlow.withAlpha(0.22f));
            g.strokePath(outerDraft, juce::PathStrokeType(0.85f));
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

                const float inner = arcR + 2.8f;
                const float outer = arcR + (isTop ? 19.5f : (isMajor ? 14.0f : 7.0f));
                auto& dst = isMajor ? (isActive ? majorActive : majorInactive)
                                    : (isActive ? minorActive : minorInactive);
                dst.startNewSubPath(cx + sinA * inner, cy + cosA * inner);
                dst.lineTo(cx + sinA * outer, cy + cosA * outer);
            }
            g.setColour(kAmberGlow.withAlpha(0.28f));
            g.strokePath(minorInactive, juce::PathStrokeType(0.9f));
            g.setColour(kAmberGlow.withAlpha(0.52f));
            g.strokePath(majorInactive, juce::PathStrokeType(1.25f));
            g.setColour(kAmberGlow.withAlpha(0.68f));
            g.strokePath(minorActive, juce::PathStrokeType(1.05f));
            g.setColour(kAmberBright.withAlpha(0.92f));
            g.strokePath(majorActive, juce::PathStrokeType(1.5f));
        }

        // Two subtle technical drafting reticles in upper-right and lower-left (matching reference)
        auto drawReticle = [&](float rx, float ry, float rad) {
            g.setColour(kAmberGlow.withAlpha(0.30f));
            g.drawEllipse(rx - rad, ry - rad, rad * 2.0f, rad * 2.0f, 0.85f);
            g.drawEllipse(rx - rad * 0.52f, ry - rad * 0.52f, rad * 1.04f, rad * 1.04f, 0.72f);
            g.drawLine(rx - rad * 1.4f, ry, rx + rad * 1.4f, ry, 0.72f);
            g.drawLine(rx, ry - rad * 1.4f, rx, ry + rad * 1.4f, 0.72f);
        };
        drawReticle(cx + maxR * 0.78f, cy - maxR * 0.66f, 14.0f);
        drawReticle(cx - maxR * 0.80f, cy + maxR * 0.68f, 16.0f);

        // Small caliper arc in upper-left
        {
            const float lx = cx - maxR * 0.78f;
            const float ly = cy - maxR * 0.68f;
            juce::Path cal;
            cal.addCentredArc(lx, ly, 14.0f, 14.0f, 0.0f, -0.6f, 1.9f, true);
            cal.startNewSubPath(lx, ly - 17.0f);
            cal.lineTo(lx, ly + 17.0f);
            cal.startNewSubPath(lx - 14.0f, ly);
            cal.lineTo(lx + 14.0f, ly);
            g.setColour(kAmberGlow.withAlpha(0.28f));
            g.strokePath(cal, juce::PathStrokeType(0.85f));
        }
    }
};
