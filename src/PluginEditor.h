#pragma once

#include "PluginProcessor.h"

/** Functional placeholder UI: an impulse-response loader on top of JUCE's generic parameter list.
    The real design replaces this class; nothing in the DSP depends on it. */
class HybridGateReverbEditor final : public juce::AudioProcessorEditor
{
public:
    explicit HybridGateReverbEditor (HybridGateReverbProcessor&);
    ~HybridGateReverbEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void chooseFile();
    void refreshFileLabel();

    HybridGateReverbProcessor& processor;
    juce::GenericAudioProcessorEditor parameters;
    juce::TextButton loadButton { "Load IR..." }, clearButton { "Clear" };
    juce::Label fileLabel;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridGateReverbEditor)
};
