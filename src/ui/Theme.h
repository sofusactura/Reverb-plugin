#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

/** Shared look for the HybridGate interface: near-black panels, cream ink, one orange accent,
    and soft raised / pressed surfaces. Matches the Sonic Shaper family. */
namespace ui
{
namespace col
{
const juce::Colour panel { 0xff161513 };
const juce::Colour panel2 { 0xff1c1b18 };
const juce::Colour cream { 0xffede4d3 };
const juce::Colour creamDim { 0xffa89f8e };
const juce::Colour creamFaint { 0xff4a463f };
const juce::Colour line { 0xff2a2824 };
const juce::Colour accent { 0xffe8893f };
} // namespace col

/** Controls are given this much extra room around their visible shape so soft shadows are not clipped. */
constexpr int shadowMargin = 8;

juce::Font sans (float size, bool bold = false);
juce::Font mono (float size);

/** Fills `shape` so it appears to sit above the panel. */
void drawRaised (juce::Graphics&, const juce::Path& shape, juce::Colour fill = col::panel2);

/** Fills `shape` so it appears pressed into the panel. */
void drawInset (juce::Graphics&, const juce::Path& shape, juce::Colour fill = col::panel2);

juce::Path pill (juce::Rectangle<float> bounds);
juce::Path rounded (juce::Rectangle<float> bounds, float radius);

/** Formats a parameter value the way the interface shows it, e.g. "8.0 kHz", "35%", "2.50 s". */
juce::String formatValue (float value, const juce::String& unit);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down, int, int, int, int, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
};
} // namespace ui
