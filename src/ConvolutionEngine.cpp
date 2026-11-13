#include "ConvolutionEngine.h"

namespace hgr
{
ConvolutionEngine::ConvolutionEngine() : juce::Thread ("HybridGate IR builder")
{
    formatManager.registerBasicFormats();
    startThread();
}

ConvolutionEngine::~ConvolutionEngine()
{
    stopThread (4000);
}

void ConvolutionEngine::prepare (double newSampleRate, int maxBlockSize, const Settings& initialSettings)
{
    const juce::ScopedLock lock (loadLock);

    sampleRate.store (newSampleRate);
    lastSettings = initialSettings;
    type.store (initialSettings.type);
    attackMs.store (initialSettings.attackMs);
    decaySeconds.store (initialSettings.decaySeconds);
    size.store (initialSettings.size);
    dirty.store (false);

    // Load synchronously here so the very first block (and offline renders) already has its IR.
    load (initialSettings);
    convolution.prepare ({ newSampleRate, (juce::uint32) juce::jmax (1, maxBlockSize), 2 });
}

void ConvolutionEngine::reset()
{
    convolution.reset();
}

void ConvolutionEngine::setSettings (const Settings& newSettings) noexcept
{
    if (newSettings == lastSettings)
        return;

    lastSettings = newSettings;
    type.store (newSettings.type);
    attackMs.store (newSettings.attackMs);
    decaySeconds.store (newSettings.decaySeconds);
    size.store (newSettings.size);
    dirty.store (true);
}

void ConvolutionEngine::process (float* left, float* right, int numSamples) noexcept
{
    float* channels[] = { left, right };
    juce::dsp::AudioBlock<float> block (channels, 2, (size_t) numSamples);
    convolution.process (juce::dsp::ProcessContextReplacing<float> (block));

    // Energy-normalised impulse responses come out quiet; bring them level with the algorithmic engine.
    block.multiplyBy (makeupGain);
}

bool ConvolutionEngine::setUserFile (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));

    if (reader == nullptr || reader->lengthInSamples < 2 || reader->sampleRate <= 0.0)
        return false;

    const int length = (int) juce::jmin ((juce::int64) (reader->sampleRate * 20.0), reader->lengthInSamples);
    juce::AudioBuffer<float> buffer (2, length);
    reader->read (&buffer, 0, length, 0, true, true);

    if (reader->numChannels < 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, length);

    {
        const juce::ScopedLock lock (userLock);
        userFile = file;
        userBuffer = std::move (buffer);
        userSampleRate = reader->sampleRate;
    }

    dirty.store (true);
    return true;
}

void ConvolutionEngine::clearUserFile()
{
    {
        const juce::ScopedLock lock (userLock);
        userFile = juce::File();
        userBuffer.setSize (0, 0);
        userSampleRate = 0.0;
    }

    dirty.store (true);
}

juce::File ConvolutionEngine::getUserFile() const
{
    const juce::ScopedLock lock (userLock);
    return userFile;
}

ConvolutionEngine::Settings ConvolutionEngine::readSettings() const noexcept
{
    return { type.load(), attackMs.load(), decaySeconds.load(), size.load() };
}

juce::AudioBuffer<float> ConvolutionEngine::build (const Settings& settings, double hostSampleRate, double& irSampleRate) const
{
    juce::AudioBuffer<float> result;

    if (settings.type >= userFileType)
    {
        const juce::ScopedLock lock (userLock);

        if (userBuffer.getNumSamples() > 1)
        {
            result.makeCopyOf (userBuffer);
            irSampleRate = userSampleRate;
        }
    }

    if (result.getNumSamples() > 1)
    {
        ir::resize (result, irSampleRate, settings.size);

        if (settings.decaySeconds < 9.99f)   // the top of the range leaves the file's own decay untouched
            ir::applyDecay (result, irSampleRate, settings.decaySeconds);
    }
    else
    {
        // "User File" with nothing loaded falls back to the hall.
        const auto builtIn = settings.type >= userFileType ? ir::hall : (ir::Type) settings.type;
        result = ir::generate (builtIn, hostSampleRate, settings.decaySeconds, settings.size);
        irSampleRate = hostSampleRate;
    }

    ir::applyAttack (result, irSampleRate, settings.attackMs);
    return result;
}

void ConvolutionEngine::load (const Settings& settings)
{
    const double hostRate = sampleRate.load();
    if (hostRate <= 0.0)
        return;

    double irRate = hostRate;
    auto buffer = build (settings, hostRate, irRate);

    const juce::ScopedLock lock (loadLock);
    convolution.loadImpulseResponse (std::move (buffer), irRate,
                                     juce::dsp::Convolution::Stereo::yes,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::yes);
}

void ConvolutionEngine::run()
{
    while (! threadShouldExit())
    {
        if (sampleRate.load() > 0.0 && dirty.exchange (false))
            load (readSettings());

        wait (25);
    }
}
} // namespace hgr
