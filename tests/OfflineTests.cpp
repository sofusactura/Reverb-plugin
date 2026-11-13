// Headless checks that run the real processor offline: no host or audio device needed.

#include "PluginProcessor.h"
#include "Presets.h"

#include <iostream>

namespace
{
int failures = 0;

void check (bool condition, const juce::String& what)
{
    std::cout << (condition ? "  ok    " : "  FAIL  ") << what << std::endl;
    if (! condition)
        ++failures;
}

struct FakePlayHead final : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setBpm (bpm);
        info.setIsPlaying (playing);
        info.setPpqPosition (ppq);
        return info;
    }

    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
};

struct Harness
{
    explicit Harness (double sr = 48000.0, int block = 512) : sampleRate (sr), blockSize (block)
    {
        processor.setPlayHead (&playHead);
        processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
    }

    void set (const juce::String& id, float value)
    {
        auto* parameter = processor.apvts.getParameter (id);
        jassert (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
    }

    /** Runs `input` through the plugin in blocks of `size` (0 = the prepared block size). */
    juce::AudioBuffer<float> render (const juce::AudioBuffer<float>& input, int size = 0)
    {
        juce::AudioBuffer<float> output;
        output.makeCopyOf (input);
        juce::MidiBuffer midi;
        juce::Random random (42);

        for (int offset = 0; offset < output.getNumSamples();)
        {
            int length = size > 0 ? size : (size < 0 ? 1 + random.nextInt (-size) : blockSize);
            length = juce::jmin (length, output.getNumSamples() - offset);

            juce::AudioBuffer<float> view (output.getArrayOfWritePointers(), 2, offset, length);
            processor.processBlock (view, midi);

            offset += length;
            playHead.ppq += length * playHead.bpm / (60.0 * sampleRate);
        }

        return output;
    }

    HybridGateReverbProcessor processor;
    FakePlayHead playHead;
    double sampleRate;
    int blockSize;
};

juce::AudioBuffer<float> impulse (int length)
{
    juce::AudioBuffer<float> buffer (2, length);
    buffer.clear();
    buffer.setSample (0, 0, 1.0f);
    buffer.setSample (1, 0, 1.0f);
    return buffer;
}

juce::AudioBuffer<float> noise (int length, float gain = 0.25f)
{
    juce::AudioBuffer<float> buffer (2, length);
    juce::Random random (7);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < length; ++i)
            buffer.setSample (ch, i, gain * (random.nextFloat() * 2.0f - 1.0f));
    return buffer;
}

bool allFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            if (! std::isfinite (buffer.getSample (ch, i)))
                return false;
    return true;
}

float rms (const juce::AudioBuffer<float>& buffer, int start, int length)
{
    return 0.5f * (buffer.getRMSLevel (0, start, length) + buffer.getRMSLevel (1, start, length));
}

float dB (float gain) { return juce::Decibels::gainToDecibels (gain, -200.0f); }

//==============================================================================
void testDryPassThrough()
{
    std::cout << "Dry pass-through" << std::endl;
    Harness h;
    h.set (ParamIDs::mix, 0.0f);
    const auto input = noise (48000);
    const auto output = h.render (input);

    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < input.getNumSamples(); ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (input.getSample (ch, i) - output.getSample (ch, i)));

    check (maxDiff < 1.0e-6f, "mix 0% returns the input unchanged (max diff " + juce::String (maxDiff, 8) + ")");
}

void testRoutingsAndAlgorithms()
{
    std::cout << "Routings and algorithms produce a sane tail" << std::endl;
    const char* routings[] = { "serial", "parallel", "algorithm", "convolution" };
    const char* algos[] = { "dark hall", "plate", "shimmer", "tides", "echoes" };

    for (int routing = 0; routing < 4; ++routing)
        for (int type = 0; type < 5; ++type)
        {
            if (routing == HybridGateReverbProcessor::convolutionOnly && type > 0)
                continue;

            Harness h;
            h.set (ParamIDs::mix, 100.0f);
            h.set (ParamIDs::routing, (float) routing);
            h.set (ParamIDs::algoType, (float) type);

            // One second of noise, then silence: measure level while driven and the decaying tail.
            auto input = noise (48000 * 4);
            input.clear (48000, 48000 * 3);
            const auto output = h.render (input);

            const float driven = dB (rms (output, 24000, 24000));
            const float tail = dB (rms (output, 48000 + 12000, 12000));
            const float late = dB (rms (output, 48000 * 3 + 24000, 24000));
            const float inputLevel = dB (rms (input, 24000, 24000));

            const auto label = juce::String (routings[routing]) + " / " + algos[type]
                             + ": driven " + juce::String (driven - inputLevel, 1) + " dB re input, tail "
                             + juce::String (tail - inputLevel, 1) + " dB, late " + juce::String (late - inputLevel, 1) + " dB";

            check (allFinite (output) && driven - inputLevel > -24.0f && driven - inputLevel < 12.0f
                       && tail > -90.0f && late < tail,
                   label);
        }
}

void testImpulseResponses()
{
    std::cout << "Every built-in impulse response" << std::endl;

    for (int type = 0; type < hgr::ir::numBuiltIn; ++type)
    {
        Harness h;
        h.set (ParamIDs::mix, 100.0f);
        h.set (ParamIDs::routing, (float) HybridGateReverbProcessor::convolutionOnly);
        h.set (ParamIDs::irType, (float) type);
        h.processor.prepareToPlay (h.sampleRate, h.blockSize);   // picks the IR up synchronously

        const auto output = h.render (impulse (48000 * 2));
        check (allFinite (output) && output.getMagnitude (0, output.getNumSamples()) > 1.0e-3f
                   && output.getMagnitude (0, output.getNumSamples()) < 8.0f,
               "IR " + juce::String (type) + " peak " + juce::String (output.getMagnitude (0, output.getNumSamples()), 3));
    }

    // Changing the IR while running must be picked up by the background builder.
    Harness h;
    h.set (ParamIDs::mix, 100.0f);
    h.set (ParamIDs::routing, (float) HybridGateReverbProcessor::convolutionOnly);
    h.set (ParamIDs::irDecay, 0.3f);
    h.render (noise (4800));
    juce::Thread::sleep (400);
    auto input = noise (48000 * 2);
    input.clear (24000, 48000 + 24000);
    const auto output = h.render (input);
    check (dB (rms (output, 24000 + 36000, 12000)) < dB (rms (output, 12000, 12000)) - 50.0f,
           "live decay change to 0.3 s shortens the tail");
}

void testStress()
{
    std::cout << "Stress: extreme settings, random block sizes" << std::endl;
    Harness h (44100.0, 256);
    h.set (ParamIDs::mix, 100.0f);
    h.set (ParamIDs::routing, (float) HybridGateReverbProcessor::serial);
    h.set (ParamIDs::algoType, (float) hgr::AlgoReverb::shimmer);
    h.set (ParamIDs::algoDecay, 30.0f);
    h.set (ParamIDs::algoShimmer, 100.0f);
    h.set (ParamIDs::algoDamping, 20000.0f);
    h.set (ParamIDs::algoSize, 100.0f);
    h.set (ParamIDs::irDecay, 10.0f);
    h.set (ParamIDs::preDelayFeedback, 90.0f);
    h.set (ParamIDs::vintage, 4.0f);
    h.set (ParamIDs::bassMono, 1.0f);
    h.set (ParamIDs::width, 200.0f);
    h.set (ParamIDs::gateOn, 1.0f);
    h.set (ParamIDs::highShelfGain, 12.0f);

    // Random block sizes from 1 to 2000 samples, far beyond the prepared 256.
    auto output = h.render (noise (44100 * 12, 0.9f), -2000);
    check (allFinite (output), "output stays finite");
    check (output.getMagnitude (0, output.getNumSamples()) < 60.0f,
           "output stays bounded (peak " + juce::String (output.getMagnitude (0, output.getNumSamples()), 2) + ")");

    h.set (ParamIDs::freeze, 1.0f);
    output = h.render (noise (44100 * 6, 0.9f), -2000);
    h.set (ParamIDs::freeze, 0.0f);
    h.set (ParamIDs::algoSize, 0.0f);
    const auto after = h.render (noise (44100 * 4, 0.9f), -2000);
    check (allFinite (output) && allFinite (after), "freeze on/off and size sweeps stay finite");
}

void testFreezeHolds()
{
    std::cout << "Freeze" << std::endl;
    Harness h;
    h.set (ParamIDs::mix, 100.0f);
    h.set (ParamIDs::routing, (float) HybridGateReverbProcessor::algorithmOnly);
    h.render (noise (48000));
    h.set (ParamIDs::freeze, 1.0f);

    juce::AudioBuffer<float> silence (2, 48000 * 10);
    silence.clear();
    const auto output = h.render (silence);
    const float early = dB (rms (output, 48000, 48000));
    const float late = dB (rms (output, 48000 * 9, 48000));
    check (early > -60.0f && std::abs (early - late) < 1.0f,
           "frozen tail holds its level (" + juce::String (early, 1) + " dB -> " + juce::String (late, 1) + " dB after 8 s)");
}

void testGate()
{
    std::cout << "Trance gate" << std::endl;
    Harness h;
    h.set (ParamIDs::mix, 100.0f);
    h.set (ParamIDs::routing, (float) HybridGateReverbProcessor::algorithmOnly);
    h.set (ParamIDs::gateOn, 1.0f);
    h.set (ParamIDs::gateRate, 3.0f);     // 1/16
    h.set (ParamIDs::gateLength, 100.0f);
    h.set (ParamIDs::gateDepth, 100.0f);
    h.set (ParamIDs::gateAttack, 0.5f);
    h.set (ParamIDs::gateRelease, 2.0f);

    for (int i = 0; i < hgr::TranceGate::maxSteps; ++i)
        h.set (ParamIDs::gateStep (i), i % 2 == 0 ? 1.0f : 0.0f);

    // 120 bpm at 48 kHz: one 1/16 step is exactly 6000 samples.
    const int step = 6000;
    const auto output = h.render (noise (step * 32), -700);

    float openLevel = 0.0f, closedLevel = 0.0f;
    for (int s = 16; s < 32; ++s)
    {
        const float level = rms (output, s * step + step / 2, step / 2 - 100);   // settled half of each step
        (s % 2 == 0 ? openLevel : closedLevel) += level / 8.0f;
    }

    check (dB (openLevel) > -50.0f && dB (closedLevel) < dB (openLevel) - 40.0f,
           "open steps " + juce::String (dB (openLevel), 1) + " dB, closed steps " + juce::String (dB (closedLevel), 1) + " dB");

    // Depth 0 % must leave the wet signal alone.
    Harness reference, shallow;
    for (auto* harness : { &reference, &shallow })
    {
        harness->set (ParamIDs::mix, 100.0f);
        harness->set (ParamIDs::routing, (float) HybridGateReverbProcessor::algorithmOnly);
    }
    shallow.set (ParamIDs::gateOn, 1.0f);
    shallow.set (ParamIDs::gateDepth, 0.0f);
    const auto a = reference.render (noise (48000)), b = shallow.render (noise (48000));
    float maxDiff = 0.0f;
    for (int i = 0; i < a.getNumSamples(); ++i)
        maxDiff = juce::jmax (maxDiff, std::abs (a.getSample (0, i) - b.getSample (0, i)));
    check (maxDiff < 1.0e-5f, "gate depth 0% is transparent");

    // With the transport stopped the gate keeps running at the last tempo.
    Harness stopped;
    stopped.playHead.playing = false;
    stopped.set (ParamIDs::mix, 100.0f);
    stopped.set (ParamIDs::routing, (float) HybridGateReverbProcessor::algorithmOnly);
    stopped.set (ParamIDs::gateOn, 1.0f);
    stopped.set (ParamIDs::gateLength, 50.0f);
    stopped.set (ParamIDs::gateRelease, 2.0f);
    const auto free = stopped.render (noise (step * 16));
    float quietest = 1.0f, loudest = 0.0f;
    for (int i = step * 8; i + 600 < free.getNumSamples(); i += 600)
    {
        const float level = rms (free, i, 600);
        quietest = juce::jmin (quietest, level);
        loudest = juce::jmax (loudest, level);
    }
    check (dB (quietest) < dB (loudest) - 30.0f, "gate free-runs while the host is stopped");
}

void testStateRoundTrip()
{
    std::cout << "State save/restore" << std::endl;
    Harness a, b;
    a.set (ParamIDs::algoDecay, 12.5f);
    a.set (ParamIDs::gateOn, 1.0f);
    a.set (ParamIDs::gateStep (3), 0.0f);
    a.set (ParamIDs::routing, (float) HybridGateReverbProcessor::parallel);

    juce::MemoryBlock state;
    a.processor.getStateInformation (state);
    b.processor.setStateInformation (state.getData(), (int) state.getSize());

    bool same = true;
    for (auto* parameter : a.processor.getParameters())
    {
        const auto* other = b.processor.getParameters()[parameter->getParameterIndex()];
        same = same && std::abs (parameter->getValue() - other->getValue()) < 1.0e-6f;
    }

    check (same, "all " + juce::String (a.processor.getParameters().size()) + " parameters survive a round trip");

    const char garbage[] = "not a preset";
    b.processor.setStateInformation (garbage, (int) sizeof (garbage));
    check (true, "garbage state is ignored without crashing");
}

void testSampleRatesAndMono()
{
    std::cout << "Sample rates and mono input" << std::endl;

    for (double rate : { 22050.0, 44100.0, 96000.0, 192000.0 })
    {
        Harness h (rate, 1024);
        h.set (ParamIDs::mix, 100.0f);
        const auto output = h.render (noise ((int) rate));
        check (allFinite (output) && rms (output, (int) rate / 2, (int) rate / 2) > 1.0e-4f,
               juce::String (rate, 0) + " Hz renders audio");
    }

    HybridGateReverbProcessor processor;
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (juce::AudioChannelSet::mono());
    layout.outputBuses.add (juce::AudioChannelSet::stereo());
    check (processor.setBusesLayout (layout), "mono in / stereo out is accepted");
    processor.prepareToPlay (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    buffer.clear();
    for (int i = 0; i < 512; ++i)
        buffer.setSample (0, i, 0.3f);
    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);
    check (allFinite (buffer) && buffer.getMagnitude (1, 0, 512) > 0.01f, "mono input reaches both outputs");
}

/** Renders the editor to a PNG, so the interface can be checked without a host. */
int writeScreenshot (const juce::File& file, int presetIndex)
{
    HybridGateReverbProcessor processor;
    processor.prepareToPlay (48000.0, 512);
    processor.loadFactoryPreset (presetIndex);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 2.0f);
    file.deleteFile();
    juce::FileOutputStream stream (file);
    return stream.openedOk() && juce::PNGImageFormat().writeImageToStream (image, stream) ? 0 : 1;
}

void testPresets()
{
    std::cout << "Factory presets" << std::endl;
    Harness h;
    const int count = (int) getFactoryPresets().size();

    for (int i = 0; i < count; ++i)
    {
        h.processor.loadFactoryPreset (i);
        h.processor.reset();
        auto input = noise (48000 * 2);
        input.clear (48000, 48000);
        const auto output = h.render (input);
        const float level = dB (rms (output, 24000, 24000)) - dB (rms (input, 24000, 24000));

        check (allFinite (output) && level > -12.0f && level < 9.0f && output.getMagnitude (0, output.getNumSamples()) < 4.0f,
               juce::String (getFactoryPresets()[(size_t) i].name) + ": level " + juce::String (level, 1) + " dB re input");
    }

    // User preset files round-trip.
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("hgr-test.hgrpreset");
    h.processor.loadFactoryPreset (count - 1);
    const float before = h.processor.apvts.getRawParameterValue (ParamIDs::highCut)->load();
    check (h.processor.savePresetFile (file), "preset file is written");
    h.processor.loadFactoryPreset (0);
    check (h.processor.loadPresetFile (file) && h.processor.apvts.getRawParameterValue (ParamIDs::highCut)->load() == before
               && h.processor.getPresetName() == "hgr-test",
           "preset file loads back with the same values");
    file.deleteFile();
}
} // namespace

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    if (argc >= 3 && juce::String (argv[1]) == "--screenshot")
        return writeScreenshot (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]), argc >= 4 ? juce::String (argv[3]).getIntValue() : 0);

    testDryPassThrough();
    testRoutingsAndAlgorithms();
    testImpulseResponses();
    testStress();
    testFreezeHolds();
    testGate();
    testStateRoundTrip();
    testSampleRatesAndMono();
    testPresets();

    std::cout << (failures == 0 ? "\nAll checks passed." : "\nFAILED checks: " + juce::String (failures)) << std::endl;
    return failures == 0 ? 0 : 1;
}
