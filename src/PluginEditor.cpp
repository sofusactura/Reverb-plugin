#include "PluginEditor.h"
#include "Presets.h"

using namespace ui;

namespace
{
// Layout grid, in base (unscaled) pixels.
constexpr int headerHeight = 56, footerHeight = 28, gateHeight = 116, dockHeight = 114, sideWidth = 270;
constexpr int W = HybridGateReverbEditor::baseWidth, H = HybridGateReverbEditor::baseHeight;
constexpr int dockTop = H - footerHeight - dockHeight;
constexpr int gateTop = dockTop - gateHeight;

const juce::Rectangle<int> presetNameArea { 262, 13, 250, 30 };
} // namespace

HybridGateReverbEditor::HybridGateReverbEditor (HybridGateReverbProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      state (p.apvts),
      routing (state, ParamIDs::routing, { "SERIAL", "PARALLEL", "ALGO", "CONV" }),
      irType (state, ParamIDs::irType),
      irAttack (state, ParamIDs::irAttack, "ATTACK"),
      irDecay (state, ParamIDs::irDecay, "DECAY"),
      irSize (state, ParamIDs::irSize, "SIZE"),
      blendAttachment (state, ParamIDs::blend, blend),
      algoType (state, ParamIDs::algoType),
      algoDecay (state, ParamIDs::algoDecay, "DECAY"),
      algoSize (state, ParamIDs::algoSize, "SIZE"),
      algoDamping (state, ParamIDs::algoDamping, "DAMPING"),
      algoDiffusion (state, ParamIDs::algoDiffusion, "DIFFUSION"),
      algoMod (state, ParamIDs::algoMod, "MOD"),
      algoShimmer (state, ParamIDs::algoShimmer, "SHIMMER"),
      freeze (state, ParamIDs::freeze, "FREEZE"),
      gateOn (state, ParamIDs::gateOn, "ON"),
      gateRate (state, ParamIDs::gateRate),
      steps (state, [] (int i) { return ParamIDs::gateStep (i); }),
      gateSteps (state, ParamIDs::gateSteps, "STEPS", true),
      gateDepth (state, ParamIDs::gateDepth, "DEPTH", true),
      gateLength (state, ParamIDs::gateLength, "LENGTH", true),
      gateAttack (state, ParamIDs::gateAttack, "ATTACK", true),
      gateRelease (state, ParamIDs::gateRelease, "RELEASE", true),
      preDelay (state, ParamIDs::preDelay, "PRE-DELAY"),
      preDelayFeedback (state, ParamIDs::preDelayFeedback, "FEEDBACK"),
      lowCut (state, ParamIDs::lowCut, "LOW CUT"),
      highCut (state, ParamIDs::highCut, "HIGH CUT"),
      lowShelfFreq (state, ParamIDs::lowShelfFreq, "LO SHELF"),
      lowShelfGain (state, ParamIDs::lowShelfGain, "LO GAIN"),
      highShelfFreq (state, ParamIDs::highShelfFreq, "HI SHELF"),
      highShelfGain (state, ParamIDs::highShelfGain, "HI GAIN"),
      width (state, ParamIDs::width, "WIDTH"),
      mix (state, ParamIDs::mix, "DRY/WET"),
      preDelaySync (state, ParamIDs::preDelaySync, "SYNC"),
      bassMono (state, ParamIDs::bassMono, "BASS MONO"),
      preDelayDiv (state, ParamIDs::preDelayDiv),
      vintage (state, ParamIDs::vintage)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (content);

    for (auto* component : std::initializer_list<juce::Component*> {
             &previousPreset, &nextPreset, &browsePresets, &savePresetButton, &routing,
             &irType, &irShape, &irAttack, &irDecay, &irSize, &loadIR, &clearIR,
             &tail, &blend,
             &algoType, &algoDecay, &algoSize, &algoDamping, &algoDiffusion, &algoMod, &algoShimmer, &freeze,
             &gateOn, &gateRate, &steps, &gateSteps, &gateDepth, &gateLength, &gateAttack, &gateRelease,
             &preDelay, &preDelayFeedback, &lowCut, &highCut, &lowShelfFreq, &lowShelfGain, &highShelfFreq, &highShelfGain,
             &width, &mix, &preDelaySync, &bassMono, &preDelayDiv, &vintage })
        content.addAndMakeVisible (component);

    previousPreset.setTitle ("Previous preset");
    nextPreset.setTitle ("Next preset");
    blend.setTitle ("Blend between convolution and algorithm");

    previousPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    browsePresets.onClick = [this] { showPresetMenu(); };
    savePresetButton.onClick = [this] { savePreset(); };
    loadIR.onClick = [this] { chooseImpulseResponse(); };
    clearIR.onClick = [this] { processor.clearUserImpulseResponse(); };

    content.setSize (baseWidth, baseHeight);
    layout();

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / (double) baseHeight);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 2, baseHeight * 2);
    setSize (baseWidth, baseHeight);

    timerCallback();
    startTimerHz (30);
}

HybridGateReverbEditor::~HybridGateReverbEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

float HybridGateReverbEditor::value (const char* parameterId) const
{
    return state.getRawParameterValue (parameterId)->load();
}

void HybridGateReverbEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseWidth));
}

void HybridGateReverbEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::panel);
}

//==============================================================================
void HybridGateReverbEditor::layout()
{
    using R = juce::Rectangle<int>;
    const int k = ui::Knob::height;

    // Header
    place (previousPreset, R (226, 13, 30, 30));
    place (nextPreset, R (518, 13, 30, 30));
    place (browsePresets, R (554, 13, 80, 30));
    place (savePresetButton, R (640, 13, 62, 30));
    routing.setBounds (W - 18 - 300, 12, 300, 32);

    // Convolution column
    const int top = headerHeight + 14;
    place (irType, R (sideWidth - 16 - 132, top, 132, 30));
    irShape.setBounds (16, top + 44, sideWidth - 32, 70);
    irAttack.setBounds (21, top + 124, 76, k);
    irDecay.setBounds (97, top + 124, 76, k);
    irSize.setBounds (173, top + 124, 76, k);
    place (loadIR, R (16, top + 232, 84, 30));
    place (clearIR, R (108, top + 232, 68, 30));

    // Centre
    const int centreLeft = sideWidth + 18, centreWidth = W - 2 * sideWidth - 36;
    tail.setBounds (centreLeft, top + 22, centreWidth, gateTop - top - 22 - 52);
    blend.setBounds (centreLeft + 58, gateTop - 42, centreWidth - 58 - 170, 28);

    // Algorithm column
    const int algoLeft = W - sideWidth + 16;
    place (algoType, R (W - 16 - 132, top, 132, 30));
    ui::Knob* algoKnobs[] = { &algoDecay, &algoSize, &algoDamping, &algoDiffusion, &algoMod, &algoShimmer };
    for (int i = 0; i < 6; ++i)
        algoKnobs[i]->setBounds (algoLeft + 5 + (i % 3) * 76, top + 40 + (i / 3) * (k + 2), 76, k);
    place (freeze, R (algoLeft, top + 244, 84, 30));

    // Gate lane
    place (gateOn, R (84, gateTop + 14, 46, 26));
    place (gateRate, R (16, gateTop + 60, 114, 30));
    ui::Knob* gateKnobs[] = { &gateSteps, &gateDepth, &gateLength, &gateAttack, &gateRelease };
    const int gateKnobsLeft = W - 12 - 5 * 68;
    for (int i = 0; i < 5; ++i)
        gateKnobs[i]->setBounds (gateKnobsLeft + i * 68, gateTop + 9, 68, k);
    steps.setBounds (148, gateTop + 14, gateKnobsLeft - 148 - 10, gateHeight - 28);

    // Dock
    ui::Knob* dockKnobs[] = { &preDelay, &preDelayFeedback, &lowCut, &highCut, &lowShelfFreq, &lowShelfGain, &highShelfFreq, &highShelfGain, &width, &mix };
    for (int i = 0; i < 10; ++i)
        dockKnobs[i]->setBounds (10 + i * 70, dockTop + 8, 70, k);

    place (preDelaySync, R (724, dockTop + 16, 84, 28));
    place (preDelayDiv, R (724, dockTop + 66, 84, 28));
    place (bassMono, R (822, dockTop + 16, 104, 28));
    place (vintage, R (822, dockTop + 66, 104, 28));
}

//==============================================================================
void HybridGateReverbEditor::paintContent (juce::Graphics& g)
{
    using R = juce::Rectangle<int>;
    g.fillAll (col::panel);

    // Dividers
    g.setColour (col::line);
    g.fillRect (0, headerHeight, W, 1);
    g.fillRect (0, gateTop, W, 1);
    g.fillRect (0, dockTop, W, 1);
    g.fillRect (0, H - footerHeight, W, 1);
    g.fillRect (sideWidth, headerHeight, 1, gateTop - headerHeight);
    g.fillRect (W - sideWidth, headerHeight, 1, gateTop - headerHeight);

    auto text = [&g] (const juce::String& s, R area, juce::Font font, juce::Colour colour,
                      juce::Justification justification = juce::Justification::centredLeft)
    {
        g.setColour (colour);
        g.setFont (font);
        g.drawText (s, area, justification, false);
    };

    // Header
    text ("HybridGate", R (18, 8, 200, 28), sans (23.0f, true), col::cream);
    text ("REVERB  //  V" JucePlugin_VersionString, R (18, 35, 200, 12), mono (9.5f), col::creamDim);

    drawInset (g, pill (presetNameArea.toFloat()));
    text (processor.getPresetName().toUpperCase(), presetNameArea.reduced (14, 0), mono (11.0f), col::cream);
    text (processor.getPresetCategory().toUpperCase(), presetNameArea.reduced (14, 0), mono (11.0f), col::creamDim,
          juce::Justification::centredRight);

    // Column titles
    const int top = headerHeight + 14;
    text ("Convolution", R (16, top, 110, 30), sans (15.0f, true), col::cream);
    text ("Algorithm", R (W - sideWidth + 16, top, 100, 30), sans (15.0f, true), col::cream);
    text (shownIR, R (16, top + 272, sideWidth - 32, 14), mono (9.5f), col::creamDim);

    // Centre
    const int centreLeft = sideWidth + 18, centreWidth = W - 2 * sideWidth - 36;
    const bool gated = value (ParamIDs::gateOn) > 0.5f;
    text (gated ? "TAIL  //  GATED" : "TAIL", R (centreLeft, top, 200, 12), mono (9.5f), col::creamDim);
    text (formatValue (juce::jmax (value (ParamIDs::irDecay), value (ParamIDs::algoDecay)), "s").toUpperCase(),
          R (centreLeft, top, centreWidth, 12), mono (9.5f), col::creamDim, juce::Justification::centredRight);

    const int blendY = gateTop - 42;
    const int blendValue = juce::roundToInt (value (ParamIDs::blend));
    text ("CONV", R (centreLeft, blendY, 50, 28), mono (9.5f), col::creamDim);
    text ("ALGO", R (centreLeft + centreWidth - 160, blendY, 44, 28), mono (9.5f), col::creamDim);
    text ("BLEND " + juce::String (100 - blendValue) + " : " + juce::String (blendValue),
          R (centreLeft + centreWidth - 112, blendY, 112, 28), mono (9.5f), col::cream, juce::Justification::centredRight);

    // Gate lane
    text ("Gate", R (16, gateTop + 12, 50, 30), sans (15.0f, true), col::cream);
    g.setColour (gated ? col::accent : col::creamFaint);
    g.fillEllipse (58.0f, (float) gateTop + 24.0f, 6.0f, 6.0f);
    text ("RATE", R (16, gateTop + 94, 114, 12), sans (9.5f), col::creamDim, juce::Justification::centred);

    // Dock
    text ("DIVISION", R (724, dockTop + 96, 84, 12), sans (9.5f), col::creamDim, juce::Justification::centred);
    text ("VINTAGE", R (822, dockTop + 96, 104, 12), sans (9.5f), col::creamDim, juce::Justification::centred);

    const int step = processor.gateStepForUi.load();
    const R readout (944, dockTop + 30, 112, 50);
    text ("STEP", readout.withHeight (16), mono (10.0f), col::creamDim);
    text (step >= 0 ? juce::String (step + 1).paddedLeft ('0', 2) : "--", readout.withHeight (16), mono (10.0f), col::cream,
          juce::Justification::centredRight);
    text ("TEMPO", readout.withTrimmedTop (18).withHeight (16), mono (10.0f), col::creamDim);
    text (juce::String (processor.bpmForUi.load(), 1), readout.withTrimmedTop (18).withHeight (16), mono (10.0f), col::cream,
          juce::Justification::centredRight);

    // Footer
    const R footer (18, H - footerHeight, W - 36, footerHeight);
    text ("HYBRIDGATE  //  2 ENGINES  //  16 STEPS", footer, mono (9.5f), col::creamDim);
    text ("CLICK OR DRAG ACROSS THE STEPS  //  DOUBLE-CLICK A KNOB TO RESET", footer, mono (9.5f), col::creamDim,
          juce::Justification::centredRight);
}

//==============================================================================
void HybridGateReverbEditor::timerCallback()
{
    unsigned pattern = 0;
    for (int i = 0; i < StepRow::numSteps; ++i)
        if (state.getRawParameterValue (ParamIDs::gateStep (i))->load() > 0.5f)
            pattern |= 1u << i;

    const bool gated = value (ParamIDs::gateOn) > 0.5f;
    const int activeSteps = juce::roundToInt (value (ParamIDs::gateSteps));
    const int step = processor.gateStepForUi.load();
    const int routingIndex = juce::roundToInt (value (ParamIDs::routing));

    TailGlyph::State tailState;
    tailState.decaySeconds = routingIndex == HybridGateReverbProcessor::convolutionOnly ? value (ParamIDs::irDecay)
                           : routingIndex == HybridGateReverbProcessor::algorithmOnly ? value (ParamIDs::algoDecay)
                           : juce::jmax (value (ParamIDs::irDecay), value (ParamIDs::algoDecay));
    tailState.gated = gated;
    tailState.gateDepth = value (ParamIDs::gateDepth) * 0.01f;
    tailState.pattern = pattern;
    tailState.steps = activeSteps;
    tailState.currentStep = step;
    tail.setState (tailState);

    TailGlyph::State irState;
    irState.decaySeconds = value (ParamIDs::irDecay);
    irState.attack = value (ParamIDs::irAttack) / 1000.0f;
    irShape.setState (irState);

    steps.setPlayState (activeSteps, gated ? step : -1);

    const auto file = processor.getUserImpulseResponse();
    const auto irText = file == juce::File() ? juce::String ("NO USER FILE LOADED") : "USER: " + file.getFileName().toUpperCase();
    const auto presetText = processor.getPresetName() + processor.getPresetCategory();
    const auto readout = juce::String (step) + juce::String (processor.bpmForUi.load(), 1) + juce::String (gated ? 1 : 0)
                       + juce::String (value (ParamIDs::blend), 0) + juce::String (tailState.decaySeconds, 2);

    clearIR.setEnabled (file != juce::File());

    if (irText != shownIR || presetText != shownPreset || readout != shownReadout)
    {
        shownIR = irText;
        shownPreset = presetText;
        shownReadout = readout;
        content.repaint();
    }
}

//==============================================================================
void HybridGateReverbEditor::stepPreset (int delta)
{
    const int count = (int) getFactoryPresets().size();
    const int current = processor.getFactoryPresetIndex();
    processor.loadFactoryPreset (current < 0 ? 0 : (current + delta + count) % count);
}

void HybridGateReverbEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto& presets = getFactoryPresets();

    // Factory presets grouped by category, in the order the categories first appear.
    juce::StringArray categories;
    for (const auto& preset : presets)
        categories.addIfNotAlreadyThere (preset.category);

    for (const auto& category : categories)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < (int) presets.size(); ++i)
            if (category == presets[(size_t) i].category)
                sub.addItem (1 + i, presets[(size_t) i].name, true, i == processor.getFactoryPresetIndex());

        menu.addSubMenu (category, sub);
    }

    const auto userFiles = HybridGateReverbProcessor::getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.hgrpreset");
    juce::PopupMenu user;
    for (int i = 0; i < userFiles.size(); ++i)
        user.addItem (1000 + i, userFiles[i].getFileNameWithoutExtension());

    menu.addSeparator();
    menu.addSubMenu ("User", user, ! userFiles.isEmpty());
    menu.addItem (900, "Save preset...");
    menu.addItem (901, "Open preset folder");

    juce::Component::SafePointer<HybridGateReverbEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (browsePresets).withParentComponent (this),
                        [safeThis, userFiles] (int result)
                        {
                            if (safeThis == nullptr || result == 0)
                                return;

                            if (result == 900)
                                safeThis->savePreset();
                            else if (result == 901)
                            {
                                const auto folder = HybridGateReverbProcessor::getUserPresetFolder();
                                folder.createDirectory();
                                folder.revealToUser();
                            }
                            else if (result >= 1000)
                                safeThis->processor.loadPresetFile (userFiles[result - 1000]);
                            else
                                safeThis->processor.loadFactoryPreset (result - 1);
                        });
}

void HybridGateReverbEditor::savePreset()
{
    const auto folder = HybridGateReverbProcessor::getUserPresetFolder();
    folder.createDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Save preset", folder.getChildFile ("My preset.hgrpreset"), "*.hgrpreset");

    juce::Component::SafePointer<HybridGateReverbEditor> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safeThis == nullptr || file == juce::File())
                                  return;

                              if (! safeThis->processor.savePresetFile (file.withFileExtension ("hgrpreset")))
                                  juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Could not save preset",
                                                                               "The preset file could not be written.");
                          });
}

void HybridGateReverbEditor::chooseImpulseResponse()
{
    auto start = processor.getUserImpulseResponse();
    if (start == juce::File())
        start = juce::File::getSpecialLocation (juce::File::userHomeDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Load an impulse response", start, "*.wav;*.aif;*.aiff;*.flac");

    juce::Component::SafePointer<HybridGateReverbEditor> safeThis (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safeThis] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safeThis == nullptr || file == juce::File())
                                  return;

                              if (! safeThis->processor.loadUserImpulseResponse (file))
                                  juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                               "Could not load impulse response",
                                                                               file.getFileName() + " is not a readable audio file.");
                          });
}
