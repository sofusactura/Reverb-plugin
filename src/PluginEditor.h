#pragma once

#include "PluginProcessor.h"
#include "ui/Widgets.h"

/** The one-page interface: both engines side by side, the gate lane across the full width,
    tone and output controls in the dock. Laid out at 1072 x 672 and scaled to the window. */
class HybridGateReverbEditor final : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit HybridGateReverbEditor (HybridGateReverbProcessor&);
    ~HybridGateReverbEditor() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    static constexpr int baseWidth = 1072, baseHeight = 672;

private:
    /** Everything is drawn inside this fixed-size component, which the editor scales. */
    struct Content final : juce::Component
    {
        explicit Content (HybridGateReverbEditor& e) : editor (e) {}
        void paint (juce::Graphics& g) override { editor.paintContent (g); }
        HybridGateReverbEditor& editor;
    };

    void timerCallback() override;
    void paintContent (juce::Graphics&);
    void layout();

    void stepPreset (int delta);
    void showPresetMenu();
    void savePreset();
    void chooseImpulseResponse();
    float value (const char* parameterId) const;

    HybridGateReverbProcessor& processor;
    ui::LookAndFeel lookAndFeel;
    Content content { *this };
    ui::APVTS& state;

    // Header
    juce::TextButton previousPreset { "<" }, nextPreset { ">" }, browsePresets { "BROWSE" }, savePresetButton { "SAVE" };
    ui::Segments routing;

    // Convolution
    ui::Choice irType;
    ui::TailGlyph irShape { true };
    ui::Knob irAttack, irDecay, irSize;
    juce::TextButton loadIR { "LOAD IR" }, clearIR { "CLEAR" };

    // Centre
    ui::TailGlyph tail;
    juce::Slider blend { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    ui::APVTS::SliderAttachment blendAttachment;

    // Algorithm
    ui::Choice algoType;
    ui::Knob algoDecay, algoSize, algoDamping, algoDiffusion, algoMod, algoShimmer;
    ui::Toggle freeze;

    // Gate
    ui::Toggle gateOn;
    ui::Choice gateRate;
    ui::StepRow steps;
    ui::Knob gateSteps, gateDepth, gateLength, gateAttack, gateRelease;

    // Dock
    ui::Knob preDelay, preDelayFeedback, lowCut, highCut, lowShelfFreq, lowShelfGain, highShelfFreq, highShelfGain, width, mix;
    ui::Toggle preDelaySync, bassMono;
    ui::Choice preDelayDiv, vintage;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String shownPreset, shownIR, shownReadout;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridGateReverbEditor)
};
