#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
constexpr float maxPreDelaySeconds = 2.0f;

// Pre-delay sync divisions, in quarter notes.
const juce::StringArray preDelayDivNames { "1/64", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4T", "1/8D", "1/4", "1/2" };
constexpr double preDelayDivQuarters[] = { 0.0625, 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 2.0 / 3.0, 0.75, 1.0, 2.0 };

// Gate step lengths, in quarter notes.
const juce::StringArray gateRateNames { "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" };
constexpr double gateRateQuarters[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

// "Vintage" modes: sample-rate and bit-depth reduction of the wet signal.
const juce::StringArray vintageNames { "Off", "Subtle", "Old", "Older", "Extreme" };
constexpr float vintageRates[] = { 0.0f, 22050.0f, 14000.0f, 9000.0f, 5000.0f };
constexpr float vintageBits[] = { 0.0f, 14.0f, 12.0f, 10.0f, 8.0f };

constexpr bool defaultGatePattern[hgr::TranceGate::maxSteps] = { true, false, true, true, true, false, true, true,
                                                                 true, false, true, false, true, true, false, true };

juce::NormalisableRange<float> skewedRange (float min, float max, float centre, float step = 0.0f)
{
    juce::NormalisableRange<float> range (min, max, step);
    range.setSkewForCentre (centre);
    return range;
}

float load (const std::atomic<float>* value) noexcept
{
    return value->load (std::memory_order_relaxed);
}

template <size_t N>
int choiceIndex (const std::atomic<float>* value, const double (&) [N]) noexcept
{
    return juce::jlimit (0, (int) N - 1, juce::roundToInt (load (value)));
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout HybridGateReverbProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const String& name, NormalisableRange<float> range, float def, const String& unit)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withLabel (unit)));
    };
    auto addChoice = [&layout] (const char* id, const String& name, const StringArray& choices, int def)
    {
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def));
    };
    auto addBool = [&layout] (const String& id, const String& name, bool def)
    {
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };
    const NormalisableRange<float> percent (0.0f, 100.0f, 0.1f);

    // Main
    addFloat (ParamIDs::mix, "Dry/Wet", percent, 35.0f, "%");
    addChoice (ParamIDs::routing, "Routing", { "Serial", "Parallel", "Algorithm", "Convolution" }, serial);
    addFloat (ParamIDs::blend, "Blend (Conv - Algo)", percent, 50.0f, "%");
    addFloat (ParamIDs::preDelay, "Pre-Delay", skewedRange (0.0f, 500.0f, 60.0f, 0.1f), 10.0f, "ms");
    addBool (ParamIDs::preDelaySync, "Pre-Delay Sync", false);
    addChoice (ParamIDs::preDelayDiv, "Pre-Delay Division", preDelayDivNames, 3);
    addFloat (ParamIDs::preDelayFeedback, "Pre-Delay Feedback", { 0.0f, 90.0f, 0.1f }, 0.0f, "%");

    // Convolution engine
    addChoice (ParamIDs::irType, "Conv IR", { "Room", "Chamber", "Hall", "Cathedral", "Plate", "Reverse", "User File" }, hgr::ir::hall);
    addFloat (ParamIDs::irAttack, "Conv Attack", skewedRange (0.0f, 500.0f, 80.0f, 0.1f), 0.0f, "ms");
    addFloat (ParamIDs::irDecay, "Conv Decay", skewedRange (0.1f, 10.0f, 2.0f, 0.01f), 2.5f, "s");
    addFloat (ParamIDs::irSize, "Conv Size", skewedRange (50.0f, 200.0f, 100.0f, 0.1f), 100.0f, "%");

    // Algorithmic engine
    addChoice (ParamIDs::algoType, "Algo Type", { "Dark Hall", "Plate", "Shimmer", "Tides", "Echoes" }, hgr::AlgoReverb::darkHall);
    addFloat (ParamIDs::algoDecay, "Algo Decay", skewedRange (0.2f, 30.0f, 4.0f, 0.01f), 4.0f, "s");
    addFloat (ParamIDs::algoSize, "Algo Size", percent, 50.0f, "%");
    addFloat (ParamIDs::algoDamping, "Algo Damping", skewedRange (500.0f, 20000.0f, 4000.0f, 1.0f), 8000.0f, "Hz");
    addFloat (ParamIDs::algoDiffusion, "Algo Diffusion", percent, 80.0f, "%");
    addFloat (ParamIDs::algoMod, "Algo Modulation", percent, 30.0f, "%");
    addFloat (ParamIDs::algoShimmer, "Algo Shimmer", percent, 40.0f, "%");
    addBool (ParamIDs::freeze, "Algo Freeze", false);

    // EQ (wet signal)
    addFloat (ParamIDs::lowCut, "EQ Low Cut", skewedRange (20.0f, 1000.0f, 150.0f, 1.0f), 20.0f, "Hz");
    addFloat (ParamIDs::highCut, "EQ High Cut", skewedRange (1000.0f, 20000.0f, 6000.0f, 1.0f), 20000.0f, "Hz");
    addFloat (ParamIDs::lowShelfFreq, "EQ Low Shelf Freq", skewedRange (40.0f, 1000.0f, 200.0f, 1.0f), 200.0f, "Hz");
    addFloat (ParamIDs::lowShelfGain, "EQ Low Shelf Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");
    addFloat (ParamIDs::highShelfFreq, "EQ High Shelf Freq", skewedRange (1000.0f, 16000.0f, 4000.0f, 1.0f), 4000.0f, "Hz");
    addFloat (ParamIDs::highShelfGain, "EQ High Shelf Gain", { -12.0f, 12.0f, 0.1f }, 0.0f, "dB");

    // Output (wet signal)
    addFloat (ParamIDs::width, "Stereo Width", { 0.0f, 200.0f, 0.1f }, 100.0f, "%");
    addBool (ParamIDs::bassMono, "Bass Mono", false);
    addChoice (ParamIDs::vintage, "Vintage", vintageNames, 0);

    // Trance gate (wet signal)
    addBool (ParamIDs::gateOn, "Gate On", false);
    addChoice (ParamIDs::gateRate, "Gate Rate", gateRateNames, 3);
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::gateSteps, 1 }, "Gate Steps",
                                                     1, hgr::TranceGate::maxSteps, hgr::TranceGate::maxSteps));
    addFloat (ParamIDs::gateDepth, "Gate Depth", percent, 100.0f, "%");
    addFloat (ParamIDs::gateLength, "Gate Length", { 5.0f, 100.0f, 0.1f }, 60.0f, "%");
    addFloat (ParamIDs::gateAttack, "Gate Attack", skewedRange (0.1f, 50.0f, 5.0f, 0.1f), 2.0f, "ms");
    addFloat (ParamIDs::gateRelease, "Gate Release", skewedRange (1.0f, 500.0f, 60.0f, 0.1f), 30.0f, "ms");

    for (int i = 0; i < hgr::TranceGate::maxSteps; ++i)
        addBool (ParamIDs::gateStep (i), "Gate Step " + String (i + 1), defaultGatePattern[i]);

    return layout;
}

//==============================================================================
HybridGateReverbProcessor::HybridGateReverbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HybridGateReverb", createLayout())
{
    auto get = [this] (const juce::String& id)
    {
        auto* value = apvts.getRawParameterValue (id);
        jassert (value != nullptr);
        return value;
    };

    raw.mix = get (ParamIDs::mix);
    raw.routing = get (ParamIDs::routing);
    raw.blend = get (ParamIDs::blend);
    raw.preDelay = get (ParamIDs::preDelay);
    raw.preDelaySync = get (ParamIDs::preDelaySync);
    raw.preDelayDiv = get (ParamIDs::preDelayDiv);
    raw.preDelayFeedback = get (ParamIDs::preDelayFeedback);
    raw.irType = get (ParamIDs::irType);
    raw.irAttack = get (ParamIDs::irAttack);
    raw.irDecay = get (ParamIDs::irDecay);
    raw.irSize = get (ParamIDs::irSize);
    raw.algoType = get (ParamIDs::algoType);
    raw.algoDecay = get (ParamIDs::algoDecay);
    raw.algoSize = get (ParamIDs::algoSize);
    raw.algoDamping = get (ParamIDs::algoDamping);
    raw.algoDiffusion = get (ParamIDs::algoDiffusion);
    raw.algoMod = get (ParamIDs::algoMod);
    raw.algoShimmer = get (ParamIDs::algoShimmer);
    raw.freeze = get (ParamIDs::freeze);
    raw.lowCut = get (ParamIDs::lowCut);
    raw.highCut = get (ParamIDs::highCut);
    raw.lowShelfFreq = get (ParamIDs::lowShelfFreq);
    raw.lowShelfGain = get (ParamIDs::lowShelfGain);
    raw.highShelfFreq = get (ParamIDs::highShelfFreq);
    raw.highShelfGain = get (ParamIDs::highShelfGain);
    raw.width = get (ParamIDs::width);
    raw.bassMono = get (ParamIDs::bassMono);
    raw.vintage = get (ParamIDs::vintage);
    raw.gateOn = get (ParamIDs::gateOn);
    raw.gateRate = get (ParamIDs::gateRate);
    raw.gateSteps = get (ParamIDs::gateSteps);
    raw.gateDepth = get (ParamIDs::gateDepth);
    raw.gateLength = get (ParamIDs::gateLength);
    raw.gateAttack = get (ParamIDs::gateAttack);
    raw.gateRelease = get (ParamIDs::gateRelease);

    for (int i = 0; i < hgr::TranceGate::maxSteps; ++i)
        raw.gateStep[i] = get (ParamIDs::gateStep (i));
}

bool HybridGateReverbProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    return out == juce::AudioChannelSet::stereo()
        && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

hgr::ConvolutionEngine::Settings HybridGateReverbProcessor::convolutionSettings() const noexcept
{
    return { juce::roundToInt (load (raw.irType)), load (raw.irAttack), load (raw.irDecay), load (raw.irSize) * 0.01f };
}

void HybridGateReverbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxChunk = juce::jmax (16, samplesPerBlock);

    dryBuffer.setSize (2, maxChunk, false, false, true);
    scratchBuffer.setSize (2, maxChunk, false, false, true);

    convolution.prepare (sampleRate, maxChunk, convolutionSettings());
    algo.prepare (sampleRate);
    gate.prepare (sampleRate);

    for (auto& line : preDelayLines)
        line.prepare ((int) (maxPreDelaySeconds * sampleRate) + 8);

    preDelayCoeff = 1.0f - std::exp (-1.0f / (0.05f * (float) sampleRate));
    bassSplitFilter.setLowPass (sampleRate, 180.0);

    reset();
}

void HybridGateReverbProcessor::reset()
{
    convolution.reset();
    algo.reset();
    gate.reset();

    for (auto& line : preDelayLines)
        line.reset();

    for (auto* filter : { &lowCutFilter, &highCutFilter, &lowShelfFilter, &highShelfFilter, &bassSplitFilter })
        filter->reset();

    // Negative values make the next block snap to its targets instead of ramping.
    preDelaySmoothed = -1.0f;
    dryGain = wetGain = blendGain = widthGain = -1.0f;
    vintagePhase = 1.0f;
    vintageHeld[0] = vintageHeld[1] = 0.0f;
    lastRouting = -1;
}

double HybridGateReverbProcessor::getTailLengthSeconds() const
{
    if (load (raw.freeze) > 0.5f)
        return std::numeric_limits<double>::infinity();

    return (double) juce::jmax (load (raw.irDecay), load (raw.algoDecay)) + maxPreDelaySeconds;
}

//==============================================================================
void HybridGateReverbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    if (getTotalNumInputChannels() < 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Host transport.
    bool hostSynced = false;
    double ppq = 0.0;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                if (*bpm > 1.0)
                    lastBpm = *bpm;

            if (const auto hostPpq = position->getPpqPosition(); hostPpq.hasValue() && position->getIsPlaying())
            {
                hostSynced = true;
                ppq = *hostPpq;
            }
        }
    }

    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    const double quartersPerSample = lastBpm / (60.0 * currentSampleRate);

    // Hosts such as FL Studio send variable (sometimes oversized) blocks; process in bounded chunks.
    for (int offset = 0; offset < numSamples;)
    {
        const int length = juce::jmin (maxChunk, numSamples - offset);
        processChunk (left + offset, right + offset, length, lastBpm, hostSynced, ppq + offset * quartersPerSample);
        offset += length;
    }
}

void HybridGateReverbProcessor::processChunk (float* left, float* right, int numSamples,
                                              double bpm, bool hostSynced, double ppq) noexcept
{
    const float sampleRate = (float) currentSampleRate;
    const float rampStep = 1.0f / (float) numSamples;

    auto* dryL = dryBuffer.getWritePointer (0);
    auto* dryR = dryBuffer.getWritePointer (1);
    std::copy (left, left + numSamples, dryL);
    std::copy (right, right + numSamples, dryR);

    //==========================================================================
    // Pre-delay with feedback. From here on left/right hold the wet signal.
    {
        float seconds = load (raw.preDelay) * 0.001f;
        if (load (raw.preDelaySync) > 0.5f)
            seconds = (float) (preDelayDivQuarters[choiceIndex (raw.preDelayDiv, preDelayDivQuarters)] * 60.0 / bpm);

        const float target = juce::jlimit (1.0f, maxPreDelaySeconds * sampleRate, seconds * sampleRate);
        const float feedback = load (raw.preDelayFeedback) * 0.01f;

        if (preDelaySmoothed < 0.0f)
            preDelaySmoothed = target;

        for (int i = 0; i < numSamples; ++i)
        {
            preDelaySmoothed += preDelayCoeff * (target - preDelaySmoothed);

            const float delayedL = preDelayLines[0].read (preDelaySmoothed);
            const float delayedR = preDelayLines[1].read (preDelaySmoothed);
            preDelayLines[0].write (left[i] + feedback * delayedL);
            preDelayLines[1].write (right[i] + feedback * delayedR);
            left[i] = delayedL;
            right[i] = delayedR;
        }
    }

    //==========================================================================
    // Reverb engines.
    {
        hgr::AlgoReverb::Params algoParams;
        algoParams.mode = juce::roundToInt (load (raw.algoType));
        algoParams.decaySeconds = load (raw.algoDecay);
        algoParams.size = load (raw.algoSize) * 0.01f;
        algoParams.dampingHz = load (raw.algoDamping);
        algoParams.diffusion = load (raw.algoDiffusion) * 0.01f;
        algoParams.modulation = load (raw.algoMod) * 0.01f;
        algoParams.shimmer = load (raw.algoShimmer) * 0.01f;
        algoParams.freeze = load (raw.freeze) > 0.5f;
        algo.setParams (algoParams);
        convolution.setSettings (convolutionSettings());

        const int routing = juce::jlimit (0, 3, juce::roundToInt (load (raw.routing)));
        if (routing != lastRouting)
        {
            // Clear whichever engine was idle so no stale tail leaks out.
            if (lastRouting == algorithmOnly)
                convolution.reset();
            if (lastRouting == convolutionOnly)
                algo.reset();

            lastRouting = routing;
        }

        const float blendTarget = load (raw.blend) * 0.01f;
        if (blendGain < 0.0f)
            blendGain = blendTarget;

        const float blendStep = (blendTarget - blendGain) * rampStep;

        if (routing == algorithmOnly)
        {
            algo.process (left, right, numSamples);
            blendGain = blendTarget;
        }
        else if (routing == convolutionOnly)
        {
            convolution.process (left, right, numSamples);
            blendGain = blendTarget;
        }
        else
        {
            auto* convL = scratchBuffer.getWritePointer (0);
            auto* convR = scratchBuffer.getWritePointer (1);

            if (routing == serial)
            {
                // Convolution feeds the algorithm; blend fades between the two stages.
                convolution.process (left, right, numSamples);
                std::copy (left, left + numSamples, convL);
                std::copy (right, right + numSamples, convR);
                algo.process (left, right, numSamples);
            }
            else
            {
                std::copy (left, left + numSamples, convL);
                std::copy (right, right + numSamples, convR);
                convolution.process (convL, convR, numSamples);
                algo.process (left, right, numSamples);
            }

            for (int i = 0; i < numSamples; ++i)
            {
                blendGain += blendStep;
                left[i] = convL[i] + blendGain * (left[i] - convL[i]);
                right[i] = convR[i] + blendGain * (right[i] - convR[i]);
            }

            blendGain = blendTarget;
        }
    }

    //==========================================================================
    // EQ.
    {
        const float eq[6] = { load (raw.lowCut), load (raw.highCut), load (raw.lowShelfFreq),
                              load (raw.lowShelfGain), load (raw.highShelfFreq), load (raw.highShelfGain) };

        if (! std::equal (std::begin (eq), std::end (eq), std::begin (eqCache)))
        {
            std::copy (std::begin (eq), std::end (eq), std::begin (eqCache));
            lowCutFilter.setHighPass (currentSampleRate, eq[0]);
            highCutFilter.setLowPass (currentSampleRate, eq[1]);
            lowShelfFilter.setLowShelf (currentSampleRate, eq[2], eq[3]);
            highShelfFilter.setHighShelf (currentSampleRate, eq[4], eq[5]);
        }

        const bool useLowCut = eq[0] > 20.5f;
        const bool useHighCut = eq[1] < 19900.0f;
        const bool useLowShelf = std::abs (eq[3]) > 0.05f;
        const bool useHighShelf = std::abs (eq[5]) > 0.05f;

        auto run = [&] (hgr::Biquad& filter)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                left[i] = filter.process (0, left[i]);
                right[i] = filter.process (1, right[i]);
            }
        };

        if (useLowCut)    run (lowCutFilter);
        if (useHighCut)   run (highCutFilter);
        if (useLowShelf)  run (lowShelfFilter);
        if (useHighShelf) run (highShelfFilter);
    }

    //==========================================================================
    // Vintage: sample-and-hold plus bit reduction.
    if (const int mode = juce::jlimit (0, 4, juce::roundToInt (load (raw.vintage))); mode > 0)
    {
        const float increment = juce::jmin (1.0f, vintageRates[mode] / sampleRate);
        const float levels = std::pow (2.0f, vintageBits[mode] - 1.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            vintagePhase += increment;
            if (vintagePhase >= 1.0f)
            {
                vintagePhase -= 1.0f;
                vintageHeld[0] = std::round (left[i] * levels) / levels;
                vintageHeld[1] = std::round (right[i] * levels) / levels;
            }

            left[i] = vintageHeld[0];
            right[i] = vintageHeld[1];
        }
    }

    //==========================================================================
    // Bass mono and stereo width.
    if (load (raw.bassMono) > 0.5f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float lowL = bassSplitFilter.process (0, left[i]);
            const float lowR = bassSplitFilter.process (1, right[i]);
            const float mono = 0.5f * (lowL + lowR);
            left[i] += mono - lowL;
            right[i] += mono - lowR;
        }
    }

    {
        const float widthTarget = load (raw.width) * 0.01f;
        if (widthGain < 0.0f)
            widthGain = widthTarget;

        if (widthTarget != 1.0f || widthGain != 1.0f)
        {
            const float step = (widthTarget - widthGain) * rampStep;
            for (int i = 0; i < numSamples; ++i)
            {
                widthGain += step;
                const float mid = 0.5f * (left[i] + right[i]);
                const float side = 0.5f * (left[i] - right[i]) * widthGain;
                left[i] = mid + side;
                right[i] = mid - side;
            }
        }

        widthGain = widthTarget;
    }

    //==========================================================================
    // Trance gate on the wet signal.
    {
        hgr::TranceGate::Params gateParams;
        gateParams.enabled = load (raw.gateOn) > 0.5f;
        gateParams.stepQuarters = gateRateQuarters[choiceIndex (raw.gateRate, gateRateQuarters)];
        gateParams.numSteps = juce::roundToInt (load (raw.gateSteps));
        gateParams.depth = load (raw.gateDepth) * 0.01f;
        gateParams.length = load (raw.gateLength) * 0.01f;
        gateParams.attackMs = load (raw.gateAttack);
        gateParams.releaseMs = load (raw.gateRelease);
        gateParams.pattern = 0;

        for (int i = 0; i < hgr::TranceGate::maxSteps; ++i)
            if (load (raw.gateStep[i]) > 0.5f)
                gateParams.pattern |= 1u << i;

        gate.process (left, right, numSamples, gateParams, bpm, hostSynced, ppq);
        gateStepForUi.store (gateParams.enabled ? gate.getCurrentStep() : -1, std::memory_order_relaxed);
        bpmForUi.store ((float) bpm, std::memory_order_relaxed);
    }

    //==========================================================================
    // Dry/wet mix (equal power).
    {
        const float mix = juce::jlimit (0.0f, 1.0f, load (raw.mix) * 0.01f);
        const float dryTarget = std::cos (mix * juce::MathConstants<float>::halfPi);
        const float wetTarget = std::sin (mix * juce::MathConstants<float>::halfPi);

        if (dryGain < 0.0f)
        {
            dryGain = dryTarget;
            wetGain = wetTarget;
        }

        const float dryStep = (dryTarget - dryGain) * rampStep;
        const float wetStep = (wetTarget - wetGain) * rampStep;
        bool finite = true;

        for (int i = 0; i < numSamples; ++i)
        {
            dryGain += dryStep;
            wetGain += wetStep;
            left[i] = dryL[i] * dryGain + left[i] * wetGain;
            right[i] = dryR[i] * dryGain + right[i] * wetGain;
            finite = finite && std::isfinite (left[i]) && std::isfinite (right[i]);
        }

        dryGain = dryTarget;
        wetGain = wetTarget;

        if (! finite)
        {
            // Should never happen, but never let a bad value reach the host: pass the dry signal and start clean.
            std::copy (dryL, dryL + numSamples, left);
            std::copy (dryR, dryR + numSamples, right);
            reset();
        }
    }
}

//==============================================================================
bool HybridGateReverbProcessor::loadUserImpulseResponse (const juce::File& file)
{
    if (! convolution.setUserFile (file))
        return false;

    if (auto* parameter = apvts.getParameter (ParamIDs::irType))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (1.0f);   // last choice = "User File"
        parameter->endChangeGesture();
    }

    return true;
}

void HybridGateReverbProcessor::clearUserImpulseResponse()
{
    convolution.clearUserFile();
}

//==============================================================================
void HybridGateReverbProcessor::loadFactoryPreset (int index)
{
    const auto& presets = getFactoryPresets();
    index = juce::jlimit (0, (int) presets.size() - 1, index);
    const auto& preset = presets[(size_t) index];

    auto set = [this] (const juce::String& id, float realValue)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (realValue));
        else
            jassertfalse;   // a preset names a parameter that does not exist
    };

    // Start from the defaults so presets only need to list what they change.
    for (auto* parameter : getParameters())
        parameter->setValueNotifyingHost (parameter->getDefaultValue());

    for (const auto& [id, value] : preset.values)
        set (id, value);

    if (preset.gatePattern != 0)
        for (int i = 0; i < hgr::TranceGate::maxSteps; ++i)
            set (ParamIDs::gateStep (i), ((preset.gatePattern >> i) & 1u) != 0 ? 1.0f : 0.0f);

    presetName = preset.name;
    presetCategory = preset.category;
    factoryPresetIndex = index;
}

juce::File HybridGateReverbProcessor::getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("HybridGate Reverb").getChildFile ("Presets");
}

juce::String HybridGateReverbProcessor::getPresetName() const { return presetName; }
juce::String HybridGateReverbProcessor::getPresetCategory() const { return presetCategory; }

bool HybridGateReverbProcessor::savePresetFile (const juce::File& file)
{
    presetName = file.getFileNameWithoutExtension();
    presetCategory = "User";
    factoryPresetIndex = -1;

    auto state = apvts.copyState();
    state.setProperty ("userIRPath", convolution.getUserFile().getFullPathName(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);

    const auto xml = state.createXml();
    return xml != nullptr && file.getParentDirectory().createDirectory().wasOk() && xml->writeTo (file);
}

bool HybridGateReverbProcessor::loadPresetFile (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return false;

    restoreState (juce::ValueTree::fromXml (*xml));
    presetName = file.getFileNameWithoutExtension();
    presetCategory = "User";
    factoryPresetIndex = -1;
    return true;
}

void HybridGateReverbProcessor::restoreState (const juce::ValueTree& state)
{
    const auto path = state.getProperty ("userIRPath").toString();
    presetName = state.getProperty ("presetName", "Init").toString();
    presetCategory = state.getProperty ("presetCategory", "Default").toString();
    factoryPresetIndex = (int) state.getProperty ("factoryPreset", -1);
    apvts.replaceState (state);

    // A missing file simply leaves the engine on its built-in fallback.
    if (path.isEmpty() || ! juce::File::isAbsolutePath (path) || ! convolution.setUserFile (juce::File (path)))
        convolution.clearUserFile();
}

void HybridGateReverbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("userIRPath", convolution.getUserFile().getFullPathName(), nullptr);
    state.setProperty ("presetName", presetName, nullptr);
    state.setProperty ("presetCategory", presetCategory, nullptr);
    state.setProperty ("factoryPreset", factoryPresetIndex, nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void HybridGateReverbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        restoreState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* HybridGateReverbProcessor::createEditor()
{
    return new HybridGateReverbEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HybridGateReverbProcessor();
}
