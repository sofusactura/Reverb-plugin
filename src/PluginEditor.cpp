#include "PluginEditor.h"

HybridGateReverbEditor::HybridGateReverbEditor (HybridGateReverbProcessor& p)
    : AudioProcessorEditor (p), processor (p), parameters (p)
{
    addAndMakeVisible (parameters);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (fileLabel);

    loadButton.onClick = [this] { chooseFile(); };
    clearButton.onClick = [this]
    {
        processor.clearUserImpulseResponse();
        refreshFileLabel();
    };

    refreshFileLabel();
    setResizable (true, true);
    setResizeLimits (420, 300, 1200, 1600);
    setSize (560, 720);
}

void HybridGateReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void HybridGateReverbEditor::resized()
{
    auto area = getLocalBounds();
    auto top = area.removeFromTop (36).reduced (6);

    loadButton.setBounds (top.removeFromLeft (90));
    top.removeFromLeft (6);
    clearButton.setBounds (top.removeFromLeft (60));
    top.removeFromLeft (8);
    fileLabel.setBounds (top);
    parameters.setBounds (area);
}

void HybridGateReverbEditor::refreshFileLabel()
{
    const auto file = processor.getUserImpulseResponse();
    fileLabel.setText (file == juce::File() ? "User IR: none (pick \"User File\" under Conv IR after loading)"
                                             : "User IR: " + file.getFileName(),
                       juce::dontSendNotification);
}

void HybridGateReverbEditor::chooseFile()
{
    auto start = processor.getUserImpulseResponse();
    if (start == juce::File())
        start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load an impulse response", start, "*.wav;*.aif;*.aiff;*.flac");

    juce::Component::SafePointer<HybridGateReverbEditor> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              if (safeThis == nullptr)
                                  return;

                              const auto file = fc.getResult();
                              if (file == juce::File())
                                  return;

                              if (! safeThis->processor.loadUserImpulseResponse (file))
                                  juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                               "Could not load impulse response",
                                                                               file.getFileName() + " is not a readable audio file.");

                              safeThis->refreshFileLabel();
                          });
}
