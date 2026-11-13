#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/IRGenerator.h"

namespace hgr
{
/** Zero-latency stereo convolution reverb. Impulse responses are built on a background
    thread whenever the settings change, then handed to the convolver without blocking audio. */
class ConvolutionEngine : private juce::Thread
{
public:
    static constexpr int userFileType = ir::numBuiltIn;
    static constexpr float makeupGain = 4.0f;

    struct Settings
    {
        int type = ir::hall;
        float attackMs = 0.0f;
        float decaySeconds = 2.5f;
        float size = 1.0f;

        bool operator== (const Settings& o) const noexcept
        {
            return type == o.type && attackMs == o.attackMs && decaySeconds == o.decaySeconds && size == o.size;
        }
    };

    ConvolutionEngine();
    ~ConvolutionEngine() override;

    void prepare (double sampleRate, int maxBlockSize, const Settings& initialSettings);
    void reset();

    /** Audio thread: lock-free. */
    void setSettings (const Settings& newSettings) noexcept;
    void process (float* left, float* right, int numSamples) noexcept;

    /** Not for the audio thread. Returns false if the file could not be read. */
    bool setUserFile (const juce::File& file);
    void clearUserFile();
    juce::File getUserFile() const;

private:
    void run() override;
    juce::AudioBuffer<float> build (const Settings& settings, double hostSampleRate, double& irSampleRate) const;
    Settings readSettings() const noexcept;
    void load (const Settings& settings);

    juce::dsp::Convolution convolution { juce::dsp::Convolution::NonUniform { 512 } };
    juce::AudioFormatManager formatManager;

    std::atomic<int> type { ir::hall };
    std::atomic<float> attackMs { 0.0f }, decaySeconds { 2.5f }, size { 1.0f };
    std::atomic<bool> dirty { false };
    std::atomic<double> sampleRate { 0.0 };
    Settings lastSettings;

    juce::CriticalSection userLock;   // guards userFile / userBuffer
    juce::CriticalSection loadLock;   // serialises prepare() against background loads
    juce::File userFile;
    juce::AudioBuffer<float> userBuffer;
    double userSampleRate = 0.0;
};
} // namespace hgr
