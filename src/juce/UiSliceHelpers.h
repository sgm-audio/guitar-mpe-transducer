#pragma once

#include <BinaryData.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <algorithm>
#include <cmath>

namespace transducer::ui {

/**
 * Draws an image using 9-slice scaling so the 4 corner regions preserve an exact
 * 1:1 aspect ratio (preventing circular rivets or corner hardware from distorting
 * into ovals at any destination aspect ratio).
 *
 * @param g            Target JUCE graphics context.
 * @param img          Source image (RGBA or RGB).
 * @param dest         Destination rectangle in logical pixels.
 * @param sourceSlice  Source border thicknesses (top, left, bottom, right) in source pixels.
 * @param borderScale  Uniform scale factor applied to both horizontal and vertical borders.
 */
inline void drawNineSlice(juce::Graphics& g,
                          const juce::Image& img,
                          juce::Rectangle<float> dest,
                          juce::BorderSize<int> sourceSlice,
                          float borderScale = 1.0f)
{
    if (!img.isValid() || dest.isEmpty())
        return;

    const int sw = img.getWidth();
    const int sh = img.getHeight();

    const int sL = std::clamp(sourceSlice.getLeft(), 0, sw / 2);
    const int sR = std::clamp(sourceSlice.getRight(), 0, sw - sL);
    const int sT = std::clamp(sourceSlice.getTop(), 0, sh / 2);
    const int sB = std::clamp(sourceSlice.getBottom(), 0, sh - sT);
    const int sMidW = std::max(0, sw - sL - sR);
    const int sMidH = std::max(0, sh - sT - sB);

    // Maintain a single uniform scale for all corners so rivets stay perfectly circular.
    float effScale = std::max(0.01f, borderScale);
    const float neededW = static_cast<float>(sL + sR) * effScale;
    const float neededH = static_cast<float>(sT + sB) * effScale;
    if (neededW > dest.getWidth() && neededW > 0.0f)
        effScale = std::min(effScale, dest.getWidth() / static_cast<float>(sL + sR));
    if (neededH > dest.getHeight() && neededH > 0.0f)
        effScale = std::min(effScale, dest.getHeight() / static_cast<float>(sT + sB));

    const float dL = static_cast<float>(sL) * effScale;
    const float dR = static_cast<float>(sR) * effScale;
    const float dT = static_cast<float>(sT) * effScale;
    const float dB = static_cast<float>(sB) * effScale;
    const float dMidW = std::max(0.0f, dest.getWidth() - dL - dR);
    const float dMidH = std::max(0.0f, dest.getHeight() - dT - dB);

    const float x0 = dest.getX();
    const float x1 = x0 + dL;
    const float x2 = dest.getRight() - dR;
    const float y0 = dest.getY();
    const float y1 = y0 + dT;
    const float y2 = dest.getBottom() - dB;

    auto drawPatch = [&](float dx, float dy, float dw, float dh,
                         int sx, int sy, int pw, int ph) {
        if (dw <= 0.01f || dh <= 0.01f || pw <= 0 || ph <= 0)
            return;
        g.drawImage(img,
                    static_cast<int>(std::floor(dx)),
                    static_cast<int>(std::floor(dy)),
                    static_cast<int>(std::ceil(dx + dw)) - static_cast<int>(std::floor(dx)),
                    static_cast<int>(std::ceil(dy + dh)) - static_cast<int>(std::floor(dy)),
                    sx, sy, pw, ph, false);
    };

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

    // 4 Corners (exact 1:1 uniform scale)
    drawPatch(x0, y0, dL, dT, 0, 0, sL, sT);
    drawPatch(x2, y0, dR, dT, sw - sR, 0, sR, sT);
    drawPatch(x0, y2, dL, dB, 0, sh - sB, sL, sB);
    drawPatch(x2, y2, dR, dB, sw - sR, sh - sB, sR, sB);

    // 4 Edges
    drawPatch(x1, y0, dMidW, dT, sL, 0, sMidW, sT);
    drawPatch(x1, y2, dMidW, dB, sL, sh - sB, sMidW, sB);
    drawPatch(x0, y1, dL, dMidH, 0, sT, sL, sMidH);
    drawPatch(x2, y1, dR, dMidH, sw - sR, sT, sR, sMidH);

    // Center
    drawPatch(x1, y1, dMidW, dMidH, sL, sT, sMidW, sMidH);
}

/**
 * Draws a horizontal 3-slice image where the left end-cap and right end-cap
 * (e.g., ComboBox bezel and arrow housing) scale strictly by destination height
 * to maintain an exact 1:1 aspect ratio at any control width.
 */
inline void drawThreeSliceHorizontal(juce::Graphics& g,
                                     const juce::Image& img,
                                     juce::Rectangle<float> dest,
                                     int leftSourceSlice,
                                     int rightSourceSlice)
{
    if (!img.isValid() || dest.isEmpty())
        return;

    const int sw = img.getWidth();
    const int sh = img.getHeight();
    if (sw <= 0 || sh <= 0)
        return;

    const int sL = std::clamp(leftSourceSlice, 0, sw / 2);
    const int sR = std::clamp(rightSourceSlice, 0, sw - sL);
    const int sMidW = std::max(1, sw - sL - sR);

    // Scale end-caps strictly by vertical ratio so end-cap features keep a 1:1 aspect ratio.
    float scale = dest.getHeight() / static_cast<float>(sh);
    const float neededW = static_cast<float>(sL + sR) * scale;
    if (neededW > dest.getWidth() && neededW > 0.0f)
        scale = dest.getWidth() / static_cast<float>(sL + sR);

    const float dL = static_cast<float>(sL) * scale;
    const float dR = static_cast<float>(sR) * scale;
    const float dMidW = std::max(0.0f, dest.getWidth() - dL - dR);

    const float x0 = dest.getX();
    const float x1 = x0 + dL;
    const float x2 = dest.getRight() - dR;
    const float y0 = dest.getY();
    const float dh = dest.getHeight();

    auto drawPatch = [&](float dx, float dw, int sx, int pw) {
        if (dw <= 0.01f || pw <= 0)
            return;
        g.drawImage(img,
                    static_cast<int>(std::floor(dx)),
                    static_cast<int>(std::floor(y0)),
                    static_cast<int>(std::ceil(dx + dw)) - static_cast<int>(std::floor(dx)),
                    static_cast<int>(std::ceil(y0 + dh)) - static_cast<int>(std::floor(y0)),
                    sx, 0, pw, sh, false);
    };

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    drawPatch(x0, dL, 0, sL);
    drawPatch(x1, dMidW, sL, sMidW);
    drawPatch(x2, dR, sw - sR, sR);
}

/**
 * Shared decoded UI texture cache backed by BinaryData.
 */
struct UiAssets {
    juce::Image chassisFrame;
    juce::Image lavaRockTile;
    juce::Image knobCap;
    juce::Image comboBoxBar;
    juce::Image activeTabLava;
    juce::Image bronzeHeaderStrip;

    static const UiAssets& get()
    {
        static const UiAssets instance = [] {
            UiAssets a;
            a.chassisFrame = juce::ImageCache::getFromMemory(
                BinaryData::chassis_frame_png, BinaryData::chassis_frame_pngSize);
            a.lavaRockTile = juce::ImageCache::getFromMemory(
                BinaryData::lava_rock_tile_png, BinaryData::lava_rock_tile_pngSize);
            a.knobCap = juce::ImageCache::getFromMemory(
                BinaryData::knob_cap_png, BinaryData::knob_cap_pngSize);
            a.comboBoxBar = juce::ImageCache::getFromMemory(
                BinaryData::combobox_bar_png, BinaryData::combobox_bar_pngSize);
            a.activeTabLava = juce::ImageCache::getFromMemory(
                BinaryData::active_tab_lava_png, BinaryData::active_tab_lava_pngSize);
            a.bronzeHeaderStrip = juce::ImageCache::getFromMemory(
                BinaryData::bronze_header_strip_png, BinaryData::bronze_header_strip_pngSize);
            return a;
        }();
        return instance;
    }
};

/**
 * Helper to pick a crisp semi-condensed bold/regular sans-serif font available on
 * Linux/macOS/Windows without relying on missing commercial typefaces.
 */
inline juce::Font makeUiFont(float height, bool bold = false, bool condensed = false)
{
    juce::FontOptions opts(height);
    if (condensed) {
        opts = opts.withName("Liberation Sans Narrow");
    } else {
        opts = opts.withName("Liberation Sans");
    }
    if (bold)
        opts = opts.withStyle("Bold");
    return juce::Font(opts);
}

/**
 * Draws glowing amber-orange text with a vertical gradient, soft 2-pass outer bloom,
 * and dark drop shadow (used for main title and hero pitch readout).
 */
inline void drawGlowingAmberText(juce::Graphics& g,
                                 const juce::String& text,
                                 const juce::Font& font,
                                 juce::Rectangle<float> bounds,
                                 juce::Justification justification,
                                 juce::Colour topColour = juce::Colour(0xFFFFD080),
                                 juce::Colour bottomColour = juce::Colour(0xFFFF6B1A),
                                 juce::Colour glowColour = juce::Colour(0xFFFF6B1A))
{
    if (text.isEmpty() || bounds.isEmpty())
        return;

    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text,
                         bounds.getX(), bounds.getY(),
                         bounds.getWidth(), bounds.getHeight(),
                         justification, 1, 0.92f);

    juce::Path textPath;
    glyphs.createPath(textPath);
    if (textPath.isEmpty())
        return;

    const auto pb = textPath.getBounds();

    // 1. Grounded dark drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.78f));
    g.fillPath(textPath, juce::AffineTransform::translation(0.0f, 2.0f));

    // 2. Two-pass warm outer glow
    g.setColour(glowColour.withAlpha(0.16f));
    g.strokePath(textPath, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(glowColour.withAlpha(0.30f));
    g.strokePath(textPath, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // 3. Vertical amber-orange gradient fill
    juce::ColourGradient grad(topColour, pb.getX(), pb.getY(),
                              bottomColour, pb.getX(), pb.getBottom(), false);
    g.setGradientFill(grad);
    g.fillPath(textPath);
}

/**
 * Draws flat, matte burnt-orange text with no drop shadow, outer bloom, or
 * highlight gradient (used for main title and hero pitch readout).
 */
inline void drawMatteOrangeText(juce::Graphics& g,
                                const juce::String& text,
                                const juce::Font& font,
                                juce::Rectangle<float> bounds,
                                juce::Justification justification,
                                juce::Colour textColour = juce::Colour(0xFFC46A34))
{
    if (text.isEmpty() || bounds.isEmpty())
        return;

    juce::GlyphArrangement glyphs;
    glyphs.addFittedText(font, text,
                         bounds.getX(), bounds.getY(),
                         bounds.getWidth(), bounds.getHeight(),
                         justification, 1, 0.92f);

    juce::Path textPath;
    glyphs.createPath(textPath);
    if (textPath.isEmpty())
        return;

    g.setColour(textColour);
    g.fillPath(textPath);
}

} // namespace transducer::ui
