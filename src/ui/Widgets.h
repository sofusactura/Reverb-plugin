#pragma once

#include "Theme.h"

namespace ui
{
using APVTS = juce::AudioProcessorValueTreeState;

/** Places a control so that `visible` is the shape the user sees; the component itself is larger to hold its shadow. */
inline void place (juce::Component& component, juce::Rectangle<int> visible)
{
    component.setBounds (visible.expanded (shadowMargin));
}

//==============================================================================
/** Rotary knob with its label and current value underneath. */
class Knob final : public juce::Component
{
public:
    Knob (APVTS& state, const juce::String& parameterId, const juce::String& labelText, bool accent = false)
        : label (labelText)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (state.getParameter (parameterId)))
            unit = parameter->getLabel();

        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.getProperties().set ("accent", accent);
        slider.setTitle (labelText);
        slider.onValueChange = [this] { repaint(); };
        addAndMakeVisible (slider);

        attachment = std::make_unique<APVTS::SliderAttachment> (state, parameterId, slider);
    }

    void resized() override
    {
        slider.setBounds (getLocalBounds().removeFromTop (70));
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().withTrimmedTop (70);
        g.setColour (col::creamDim);
        g.setFont (sans (9.5f));
        g.drawText (label, area.removeFromTop (12), juce::Justification::centred, false);
        g.setColour (col::cream);
        g.setFont (mono (9.5f));
        g.drawText (formatValue ((float) slider.getValue(), unit), area.removeFromTop (14), juce::Justification::centred, false);
    }

    static constexpr int height = 98;

private:
    juce::Slider slider;
    juce::String label, unit;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
};

//==============================================================================
/** Drop-down bound to a choice parameter, drawn as a raised pill. */
class Choice final : public juce::Component
{
public:
    Choice (APVTS& state, const juce::String& parameterId)
    {
        if (auto* parameter = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (parameterId)))
            box.addItemList (parameter->choices, 1);

        addAndMakeVisible (box);
        attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, parameterId, box);
    }

    void resized() override { box.setBounds (getLocalBounds()); }

private:
    juce::ComboBox box;
    std::unique_ptr<APVTS::ComboBoxAttachment> attachment;
};

//==============================================================================
/** On/off pill bound to a bool parameter. */
class Toggle final : public juce::TextButton
{
public:
    Toggle (APVTS& state, const juce::String& parameterId, const juce::String& text) : juce::TextButton (text)
    {
        setClickingTogglesState (true);
        attachment = std::make_unique<APVTS::ButtonAttachment> (state, parameterId, *this);
    }

private:
    std::unique_ptr<APVTS::ButtonAttachment> attachment;
};

//==============================================================================
/** Row of mutually exclusive pills in a pressed well, bound to a choice parameter. */
class Segments final : public juce::Component
{
public:
    Segments (APVTS& state, const juce::String& parameterId, const juce::StringArray& labels)
        : attachment (*state.getParameter (parameterId), [this] (float value) { select (juce::roundToInt (value)); })
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* button = buttons.add (new juce::TextButton (labels[i]));
            button->getProperties().set ("segment", true);
            button->setClickingTogglesState (true);
            button->setRadioGroupId (1);
            button->onClick = [this, i, button]
            {
                if (button->getToggleState())
                    attachment.setValueAsCompleteGesture ((float) i);
            };
            addAndMakeVisible (button);
        }

        attachment.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        drawInset (g, pill (getLocalBounds().toFloat()));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);
        const int width = area.getWidth() / juce::jmax (1, buttons.size());

        for (auto* button : buttons)
            place (*button, area.removeFromLeft (width).reduced (1, 0));
    }

private:
    void select (int index)
    {
        if (auto* button = buttons[index])
            button->setToggleState (true, juce::dontSendNotification);
    }

    juce::OwnedArray<juce::TextButton> buttons;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** The 16 gate steps. Click to toggle, drag across to paint. */
class StepRow final : public juce::Component
{
public:
    static constexpr int numSteps = 16;

    StepRow (APVTS& state, const std::function<juce::String (int)>& parameterIdForStep)
    {
        for (int i = 0; i < numSteps; ++i)
        {
            attachments.add (new juce::ParameterAttachment (*state.getParameter (parameterIdForStep (i)), [this, i] (float value)
            {
                open[i] = value > 0.5f;
                repaint();
            }));
            attachments.getLast()->sendInitialUpdate();
        }

        setTitle ("Gate steps");
    }

    /** `active` = how many steps the pattern uses; `current` = the step playing now, or -1. */
    void setPlayState (int active, int current)
    {
        if (active != activeSteps || current != currentStep)
        {
            activeSteps = active;
            currentStep = current;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        drawInset (g, rounded (getLocalBounds().toFloat(), 12.0f));

        for (int i = 0; i < numSteps; ++i)
        {
            const auto cell = cellBounds (i);
            const bool used = i < activeSteps;
            const bool playing = i == currentStep;

            g.setFont (mono (8.5f));
            g.setColour (playing ? col::accent : col::creamDim.withAlpha (used ? 1.0f : 0.4f));
            g.drawText (juce::String (i + 1).paddedLeft ('0', 2), cell.withY (cell.getY() - 15).withHeight (12),
                        juce::Justification::centred, false);

            g.setColour (open[i] ? col::cream.withAlpha (used ? 1.0f : 0.25f) : col::panel);
            g.fillRoundedRectangle (cell.toFloat(), 3.0f);

            if (playing)
            {
                g.setColour (col::accent);
                g.drawRoundedRectangle (cell.toFloat().reduced (1.0f), 3.0f, 2.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& event) override
    {
        const int index = stepAt (event.x);
        if (index < 0)
            return;

        paintValue = ! open[index];
        attachments[index]->setValueAsCompleteGesture (paintValue ? 1.0f : 0.0f);
    }

    void mouseDrag (const juce::MouseEvent& event) override
    {
        const int index = stepAt (event.x);
        if (index >= 0 && open[index] != paintValue)
            attachments[index]->setValueAsCompleteGesture (paintValue ? 1.0f : 0.0f);
    }

private:
    juce::Rectangle<int> cellBounds (int index) const
    {
        const auto area = getLocalBounds().reduced (12, 10).withTrimmedTop (15);
        const float width = (float) area.getWidth() / (float) numSteps;
        return { area.getX() + juce::roundToInt ((float) index * width) + 1, area.getY(),
                 juce::jmax (4, juce::roundToInt (width) - 3), area.getHeight() };
    }

    int stepAt (int x) const
    {
        const auto area = getLocalBounds().reduced (12, 10);
        if (x < area.getX() || x >= area.getRight())
            return -1;

        return juce::jlimit (0, numSteps - 1, (x - area.getX()) * numSteps / area.getWidth());
    }

    juce::OwnedArray<juce::ParameterAttachment> attachments;
    bool open[numSteps] {};
    bool paintValue = true;
    int activeSteps = numSteps, currentStep = -1;
};

//==============================================================================
/** Picture of a decaying reverb tail. With a gate pattern it dims the bars that fall on closed steps. */
class TailGlyph final : public juce::Component
{
public:
    struct State
    {
        float decaySeconds = 2.0f, attack = 0.0f, gateDepth = 0.0f;
        unsigned pattern = 0xffff;
        int steps = 16, currentStep = -1;
        bool gated = false;

        bool operator== (const State& o) const
        {
            return decaySeconds == o.decaySeconds && attack == o.attack && gateDepth == o.gateDepth
                && pattern == o.pattern && steps == o.steps && currentStep == o.currentStep && gated == o.gated;
        }
    };

    explicit TailGlyph (bool drawWell = false) : well (drawWell) { setInterceptsMouseClicks (false, false); }

    void setState (const State& newState)
    {
        if (! (newState == state))
        {
            state = newState;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        if (well)
        {
            g.setColour (col::panel);
            g.fillRoundedRectangle (area, 10.0f);
            drawInset (g, rounded (area, 10.0f), col::panel);
            area = area.reduced (8.0f, 6.0f);
        }

        const float spacing = well ? 3.0f : 4.0f;
        const int count = juce::jmax (1, (int) (area.getWidth() / spacing));
        const float middle = area.getCentreY(), reach = area.getHeight() * 0.5f - 1.0f;

        // Longer decays fill more of the width: the picture spans a fixed 10 seconds on a curved scale.
        const float fill = juce::jlimit (0.06f, 1.0f, std::sqrt (state.decaySeconds / 20.0f) * 1.25f);
        const float attackBars = state.attack * (float) count;
        juce::Random random (11);
        juce::Path openBars, closedBars;

        for (int i = 0; i < count; ++i)
        {
            const float t = (float) i / (float) count;
            float height = reach * std::exp (-4.5f * t / fill) * (0.4f + 0.6f * random.nextFloat());

            if ((float) i < attackBars)
                height *= ((float) i + 1.0f) / (attackBars + 1.0f);

            const float x = area.getX() + ((float) i + 0.5f) * spacing;
            const int step = juce::jlimit (0, 15, (int) (t * (float) state.steps));
            const bool isOpen = ! state.gated || ((state.pattern >> step) & 1u) != 0;

            auto& path = isOpen ? openBars : closedBars;
            path.startNewSubPath (x, middle - height - 1.0f);
            path.lineTo (x, middle + height + 1.0f);
        }

        const juce::PathStrokeType stroke (well ? 1.5f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        g.setColour (col::creamFaint.interpolatedWith (col::cream, 1.0f - state.gateDepth));
        g.strokePath (closedBars, stroke);
        g.setColour (col::cream);
        g.strokePath (openBars, stroke);

        if (state.gated && state.currentStep >= 0 && state.steps > 0)
        {
            const float x = area.getX() + area.getWidth() * ((float) state.currentStep + 0.5f) / (float) state.steps;
            g.setColour (col::accent);
            g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
        }
    }

private:
    State state;
    bool well;
};
} // namespace ui
