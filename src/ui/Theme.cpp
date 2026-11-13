#include "Theme.h"
#include "BinaryData.h"

namespace ui
{
namespace
{
struct Typefaces
{
    juce::Typeface::Ptr semiBold = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoSemiBold_ttf, BinaryData::ArchivoSemiBold_ttfSize);
    juce::Typeface::Ptr bold = juce::Typeface::createSystemTypefaceFor (BinaryData::ArchivoBold_ttf, BinaryData::ArchivoBold_ttfSize);
    juce::Typeface::Ptr mono = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexMonoMedium_ttf, BinaryData::IBMPlexMonoMedium_ttfSize);
};

Typefaces& typefaces()
{
    static Typefaces instance;
    return instance;
}

juce::Font makeFont (const juce::Typeface::Ptr& typeface, float size)
{
    return typeface != nullptr ? juce::Font (juce::FontOptions (typeface).withPointHeight (size))
                               : juce::Font (juce::FontOptions().withPointHeight (size));
}
} // namespace

juce::Font sans (float size, bool bold) { return makeFont (bold ? typefaces().bold : typefaces().semiBold, size); }
juce::Font mono (float size) { return makeFont (typefaces().mono, size); }

juce::Path pill (juce::Rectangle<float> bounds)
{
    return rounded (bounds, bounds.getHeight() * 0.5f);
}

juce::Path rounded (juce::Rectangle<float> bounds, float radius)
{
    juce::Path path;
    path.addRoundedRectangle (bounds, radius);
    return path;
}

void drawRaised (juce::Graphics& g, const juce::Path& shape, juce::Colour fill)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.85f), 7, { 3, 3 }).drawForPath (g, shape);
    juce::DropShadow (juce::Colours::white.withAlpha (0.06f), 6, { -2, -2 }).drawForPath (g, shape);
    g.setColour (fill);
    g.fillPath (shape);
}

void drawInset (juce::Graphics& g, const juce::Path& shape, juce::Colour fill)
{
    g.setColour (fill);
    g.fillPath (shape);

    // Inner shadow: blur the outline and keep only the part that falls inside the shape.
    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (shape);

    juce::Path ring;
    juce::PathStrokeType (3.0f).createStrokedPath (ring, shape);
    juce::DropShadow (juce::Colours::black.withAlpha (0.9f), 7, { 3, 3 }).drawForPath (g, ring);
    juce::DropShadow (juce::Colours::white.withAlpha (0.05f), 5, { -2, -2 }).drawForPath (g, ring);
}

juce::String formatValue (float value, const juce::String& unit)
{
    if (unit == "Hz")
        return value >= 1000.0f ? juce::String (value / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (value)) + " Hz";
    if (unit == "%")
        return juce::String (juce::roundToInt (value)) + "%";
    if (unit == "ms")
        return (value < 10.0f ? juce::String (value, 1) : juce::String (juce::roundToInt (value))) + " ms";
    if (unit == "s")
        return juce::String (value, value < 10.0f ? 2 : 1) + " s";
    if (unit == "dB")
        return (value > 0.05f ? "+" : "") + juce::String (value, 1) + " dB";

    return juce::String (juce::roundToInt (value));
}

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, col::panel);
    setColour (juce::ComboBox::textColourId, col::cream);
    setColour (juce::PopupMenu::backgroundColourId, col::panel2);
    setColour (juce::PopupMenu::textColourId, col::cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::creamFaint);
    setColour (juce::PopupMenu::highlightedTextColourId, col::cream);
    setColour (juce::Label::textColourId, col::cream);
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    if (typefaces().semiBold != nullptr && font.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typefaces().semiBold;

    return juce::LookAndFeel_V4::getTypefaceForFont (font);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                                    float startAngle, float endAngle, juce::Slider& slider)
{
    const auto centre = juce::Rectangle<int> (x, y, width, height).toFloat().getCentre();
    const float radius = 28.0f;
    const float angle = startAngle + position * (endAngle - startAngle);

    juce::Path body, well;
    body.addEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    well.addEllipse (centre.x - 16.0f, centre.y - 16.0f, 32.0f, 32.0f);
    drawRaised (g, body);
    drawInset (g, well);

    const juce::PathStrokeType stroke (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track, value;
    track.addCentredArc (centre.x, centre.y, 24.0f, 24.0f, 0.0f, startAngle, endAngle, true);
    value.addCentredArc (centre.x, centre.y, 24.0f, 24.0f, 0.0f, startAngle, angle, true);

    g.setColour (col::creamFaint);
    g.strokePath (track, stroke);
    g.setColour (slider.getProperties()["accent"] ? col::accent : col::cream);
    g.strokePath (value, stroke);

    juce::Path pointer;
    pointer.startNewSubPath (centre.getPointOnCircumference (7.0f, angle));
    pointer.lineTo (centre.getPointOnCircumference (13.0f, angle));
    g.setColour (col::cream.withAlpha (slider.isEnabled() ? 1.0f : 0.4f));
    g.strokePath (pointer, stroke);
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const auto area = juce::Rectangle<int> (x, y, width, height).toFloat();
    drawInset (g, pill (area.withSizeKeepingCentre (area.getWidth() + 14.0f, 14.0f)));

    juce::Path thumb;
    thumb.addEllipse (sliderPos - 11.0f, area.getCentreY() - 11.0f, 22.0f, 22.0f);
    juce::DropShadow (juce::Colours::black.withAlpha (0.8f), 5, { 2, 2 }).drawForPath (g, thumb);
    g.setColour (col::cream);
    g.fillPath (thumb);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool, bool down)
{
    const auto shape = pill (button.getLocalBounds().toFloat().reduced ((float) shadowMargin));
    const bool pressed = down || button.getToggleState();

    if (button.getProperties()["segment"])
    {
        if (pressed)
            drawRaised (g, shape);   // segments sit in a pressed well, so the chosen one pops out
    }
    else if (pressed)
        drawInset (g, shape);
    else
        drawRaised (g, shape);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool highlighted, bool)
{
    const bool lit = button.getToggleState() || ! button.getClickingTogglesState() || highlighted;
    g.setColour ((lit ? col::cream : col::creamDim).withAlpha (button.isEnabled() ? 1.0f : 0.4f));
    g.setFont (sans (button.getProperties()["segment"] ? 10.5f : 11.5f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool down, int, int, int, int, juce::ComboBox&)
{
    const auto area = juce::Rectangle<int> (width, height).toFloat().reduced ((float) shadowMargin);

    if (down)
        drawInset (g, pill (area));
    else
        drawRaised (g, pill (area));

    juce::Path chevron;
    const float cx = area.getRight() - 15.0f, cy = area.getCentreY();
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (col::cream);
    g.strokePath (chevron, juce::PathStrokeType (1.6f));
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&) { return sans (11.5f); }
juce::Font LookAndFeel::getPopupMenuFont() { return sans (12.0f); }

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (box.getLocalBounds().reduced (shadowMargin).withTrimmedLeft (12).withTrimmedRight (24));
    label.setFont (getComboBoxFont (box));
}
} // namespace ui
