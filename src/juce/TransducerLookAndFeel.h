#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

class TransducerLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    static inline const juce::Colour kBg{0xFF0D0F12};
    static inline const juce::Colour kCard{0xFF14171C};
    static inline const juce::Colour kBorder{0xFF232832};
    static inline const juce::Colour kAccent{0xFF38BDF8};
    static inline const juce::Colour kText{0xFFE5E7EB};
    static inline const juce::Colour kTextDim{0xFF9CA3AF};

    TransducerLookAndFeel()
    {
        setColour(juce::Slider::rotarySliderFillColourId, kAccent);
        setColour(juce::Slider::rotarySliderOutlineColourId, kBorder);
        setColour(juce::Slider::thumbColourId, kAccent);
        setColour(juce::Slider::textBoxTextColourId, kText);
        setColour(juce::Slider::textBoxBackgroundColourId, kBg);
        setColour(juce::Slider::textBoxOutlineColourId, kBorder);

        setColour(juce::ComboBox::backgroundColourId, kBg);
        setColour(juce::ComboBox::textColourId, kText);
        setColour(juce::ComboBox::outlineColourId, kBorder);
        setColour(juce::ComboBox::arrowColourId, kAccent);

        setColour(juce::PopupMenu::backgroundColourId, kCard);
        setColour(juce::PopupMenu::textColourId, kText);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.22f));
        setColour(juce::PopupMenu::highlightedTextColourId, kText);
    }

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPos,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider&) override
    {
        const float radius = static_cast<float>(std::min(width, height)) * 0.5f - 6.0f;
        const float cx = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        const float cy = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float stroke = 3.0f;

        // Track arc
        juce::Path track;
        track.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(kBorder);
        g.strokePath(track, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        // Active value arc
        if (sliderPos > 0.001f) {
            juce::Path fill;
            fill.addCentredArc(cx, cy, radius, radius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour(kAccent);
            g.strokePath(fill, juce::PathStrokeType(stroke, juce::PathStrokeType::curved,
                                                    juce::PathStrokeType::rounded));
        }

        // Inner dial body & pointer
        const float inner_r = std::max(radius - 5.0f, 4.0f);
        g.setColour(kBg);
        g.fillEllipse(cx - inner_r, cy - inner_r, inner_r * 2.0f, inner_r * 2.0f);
        g.setColour(kBorder);
        g.drawEllipse(cx - inner_r, cy - inner_r, inner_r * 2.0f, inner_r * 2.0f, 1.0f);

        juce::Path ptr;
        ptr.addRoundedRectangle(-1.25f, -inner_r + 2.0f, 2.5f, inner_r * 0.45f, 1.0f);
        g.setColour(kAccent);
        g.fillPath(ptr, juce::AffineTransform::rotation(angle).translated(cx, cy));
    }

    void drawComboBox(juce::Graphics& g,
                      int width, int height,
                      bool isButtonDown,
                      int, int, int, int,
                      juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<float>(0.0f, 0.0f,
                                                   static_cast<float>(width),
                                                   static_cast<float>(height)).reduced(0.5f);
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(bounds, 4.0f);

        g.setColour(isButtonDown || box.hasKeyboardFocus(false)
                        ? kAccent
                        : box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        const float ax = static_cast<float>(width) - 16.0f;
        const float ay = static_cast<float>(height) * 0.5f - 2.0f;
        juce::Path arrow;
        arrow.addTriangle(ax, ay, ax + 8.0f, ay, ax + 4.0f, ay + 5.0f);
        g.setColour(kAccent);
        g.fillPath(arrow);
    }
};
