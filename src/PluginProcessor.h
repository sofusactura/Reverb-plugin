#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "ConvolutionEngine.h"
#include "dsp/AlgoReverb.h"
#include "dsp/TranceGate.h"
#include "dsp/Utils.h"

namespace ParamIDs
{
// Never rename these: hosts store automation and presets against them.
inline constexpr auto mix = "mix";
inline constexpr auto routing = "routing";
inline constexpr auto blend = "blend";
inline constexpr auto preDelay = "preDelay";
inline constexpr auto preDelaySync = "preDelaySync";
inline constexpr auto preDelayDiv = "preDelayDiv";
inline constexpr auto preDelayFeedback = "preDelayFeedback";

inline constexpr auto irType = "irType";
inline constexpr auto irAttack = "irAttack";
inline constexpr auto irDecay = "irDecay";
inline constexpr auto irSize = "irSize";

inline constexpr auto algoType = "algoType";
inline constexpr auto algoDecay = "algoDecay";
inline constexpr auto algoSize = "algoSize";
inline constexpr auto algoDamping = "algoDamping";
inline constexpr auto algoDiffusion = "algoDiffusion";
inline constexpr auto algoMod = "algoMod";
inline constexpr auto algoShimmer = "algoShimmer";
inline constexpr auto freeze = "freeze";

inline constexpr auto lowCut = "lowCut";
inline constexpr auto highCut = "highCut";
inline constexpr auto lowShelfFreq = "lowShelfFreq";
inline constexpr auto lowShelfGain = "lowShelfGain";
inline constexpr auto highShelfFreq = "highShelfFreq";
inline constexpr auto highShelfGain = "highShelfGain";

inline constexpr auto width = "width";
inline constexpr auto bassMono = "bassMono";
inline constexpr auto vintage = "vintage";

inline constexpr auto gateOn = "gateOn";
inline constexpr auto gateRate = "gateRate";
inline constexpr auto gateSteps = "gateSteps";
inline constexpr auto gateDepth = "gateDepth";
inline constexpr auto gateLength = "gateLength";
inline constexpr auto gateAttack = "gateAttack";
inline constexpr auto gateRelease = "gateRelease";

inline juce::String gateStep (int index) { return "gateStep" + juce::String (index + 1); }
} // namespace ParamIDs

class HybridGateReverbProcessor final : public juce::AudioProcessor
{
public:
    enum Routing { serial = 0, parallel, algorithmOnly, convolutionOnly };

    HybridGateReverbProcessor();
    ~HybridGateReverbProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Loads an impulse response from disk and switches the convolution engine to it. */
    bool loadUserImpulseResponse (const juce::File& file);
    void clearUserImpulseResponse();
    juce::File getUserImpulseResponse() const { return convolution.getUserFile(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct Raw
    {
        std::atomic<float>* mix, *routing, *blend, *preDelay, *preDelaySync, *preDelayDiv, *preDelayFeedback;
        std::atomic<float>* irType, *irAttack, *irDecay, *irSize;
        std::atomic<float>* algoType, *algoDecay, *algoSize, *algoDamping, *algoDiffusion, *algoMod, *algoShimmer, *freeze;
        std::atomic<float>* lowCut, *highCut, *lowShelfFreq, *lowShelfGain, *highShelfFreq, *highShelfGain;
        std::atomic<float>* width, *bassMono, *vintage;
        std::atomic<float>* gateOn, *gateRate, *gateSteps, *gateDepth, *gateLength, *gateAttack, *gateRelease;
        std::atomic<float>* gateStep[hgr::TranceGate::maxSteps];
    };

    hgr::ConvolutionEngine::Settings convolutionSettings() const noexcept;
    void processChunk (float* left, float* right, int numSamples, double bpm, bool hostSynced, double ppq) noexcept;

    Raw raw {};

    hgr::ConvolutionEngine convolution;
    hgr::AlgoReverb algo;
    hgr::TranceGate gate;
    hgr::DelayLine preDelayLines[2];
    hgr::Biquad lowCutFilter, highCutFilter, lowShelfFilter, highShelfFilter, bassSplitFilter;

    juce::AudioBuffer<float> dryBuffer, scratchBuffer;
    double currentSampleRate = 44100.0;
    int maxChunk = 512;

    float preDelaySmoothed = 1.0f, preDelayCoeff = 0.001f;
    float dryGain = 1.0f, wetGain = 0.0f, blendGain = 0.5f, widthGain = 1.0f;
    float eqCache[6] { -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, -1.0f };
    float vintagePhase = 1.0f, vintageHeld[2] {};
    int lastRouting = -1;
    double lastBpm = 120.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HybridGateReverbProcessor)
};
