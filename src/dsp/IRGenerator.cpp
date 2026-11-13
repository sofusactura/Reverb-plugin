#include "IRGenerator.h"
#include "Utils.h"

namespace hgr::ir
{
namespace
{
struct Space
{
    double preDelayMs, earlyMs;
    int earlyCount;
    float earlyGain;
    double lowDecay, highDecay;   // decay multipliers relative to the mid band
    double densityMs, brightnessHz;
};

constexpr Space spaces[] = {
    { 0.0,  25.0,  14, 0.9f, 1.00, 0.45, 10.0,  9000.0 },    // room
    { 4.0,  40.0,  16, 0.8f, 1.10, 0.55, 20.0,  11000.0 },   // chamber
    { 12.0, 80.0,  20, 0.7f, 1.25, 0.40, 50.0,  10000.0 },   // hall
    { 25.0, 140.0, 24, 0.6f, 1.50, 0.30, 120.0, 7000.0 },    // cathedral
    { 0.0,  0.0,   0,  0.0f, 0.90, 0.75, 2.0,   16000.0 },   // plate
    { 0.0,  0.0,   0,  0.0f, 1.00, 1.00, 2.0,   12000.0 },   // reverse
};

constexpr double maxSeconds = 12.0;

float onePole (double sampleRate, double freq)
{
    return (float) (1.0 - std::exp (-2.0 * pi * juce::jmin (freq, sampleRate * 0.45) / sampleRate));
}
} // namespace

juce::AudioBuffer<float> generate (Type type, double sampleRate, double decaySeconds, double size)
{
    type = (Type) juce::jlimit (0, (int) numBuiltIn - 1, (int) type);
    size = juce::jlimit (0.25, 4.0, size);
    decaySeconds = juce::jlimit (0.05, maxSeconds, decaySeconds);

    const auto& space = spaces[type];
    const bool isReverse = type == reverse;

    // Everything is laid out on a time axis scaled by `size`: larger spaces have later
    // reflections and a darker tone, while the decay time itself stays as requested.
    const double rate = sampleRate * size;   // filters are tuned against this, so larger spaces sound darker
    const double preDelay = space.preDelayMs * 0.001 * size;
    const double bodySeconds = isReverse ? juce::jmin (decaySeconds * 0.5, 4.0) : decaySeconds;
    const int length = juce::jmax (64, (int) ((preDelay + bodySeconds) * sampleRate));
    const int start = (int) (preDelay * sampleRate);

    juce::AudioBuffer<float> ir (2, length);
    ir.clear();

    const float lowCoeff = onePole (rate, 250.0);
    const float highCoeff = onePole (rate, 4000.0);
    const float toneCoeff = onePole (rate, space.brightnessHz);
    const double densitySamples = juce::jmax (1.0, space.densityMs * 0.001 * sampleRate * size);
    const double k = -6.9078 / sampleRate;   // ln(0.001) per second

    for (int ch = 0; ch < 2; ++ch)
    {
        XorShift rng (0x1234567u * (uint32_t) (type + 1) + 0x9e3779b9u * (uint32_t) (ch + 1));
        auto* data = ir.getWritePointer (ch);
        float low = 0.0f, mid4k = 0.0f, tone1 = 0.0f, tone2 = 0.0f;

        for (int n = start; n < length; ++n)
        {
            const double t = (double) (n - start);
            const float white = rng.bipolar();

            low += lowCoeff * (white - low);
            mid4k += highCoeff * (white - mid4k);

            float sample;

            if (isReverse)
            {
                const double remaining = (double) (length - n) / (double) (length - start);
                sample = white * (float) std::exp (-5.0 * remaining);
            }
            else
            {
                const float envMid = (float) std::exp (k * t / decaySeconds);
                const float envLow = (float) std::exp (k * t / (decaySeconds * space.lowDecay));
                const float envHigh = (float) std::exp (k * t / (decaySeconds * space.highDecay));
                const float density = (float) juce::jmin (1.0, t / densitySamples);

                sample = density * (low * envLow * 2.0f + (mid4k - low) * envMid + (white - mid4k) * envHigh);
            }

            tone1 += toneCoeff * (sample - tone1);
            tone2 += toneCoeff * (tone1 - tone2);
            data[n] = tone2;
        }

        // Early reflections.
        for (int i = 0; i < space.earlyCount; ++i)
        {
            const double position = 0.03 + 0.97 * (double) rng.uniform();
            const int index = start + (int) (position * space.earlyMs * 0.001 * sampleRate * size);
            const float gain = space.earlyGain * (1.0f - 0.65f * (float) position) * (rng.uniform() < 0.5f ? -1.0f : 1.0f);

            if (index < length)
                data[index] += gain;
        }

        if (start < length)
            data[start] += isReverse ? 0.0f : 1.0f;   // direct arrival

        // Short fade so the truncated tail never clicks.
        const int fade = juce::jmin (length, isReverse ? (int) (0.004 * sampleRate) : length / 20);
        for (int i = 0; i < fade; ++i)
            data[length - 1 - i] *= (float) i / (float) fade;
    }

    return ir;
}

void resize (juce::AudioBuffer<float>& ir, double sampleRate, double factor)
{
    factor = juce::jlimit (0.25, 4.0, factor);
    const int oldLength = ir.getNumSamples();

    if (std::abs (factor - 1.0) < 1.0e-3 || oldLength < 2)
        return;

    const int newLength = juce::jlimit (2, (int) (maxSeconds * sampleRate), (int) ((double) oldLength * factor));
    juce::AudioBuffer<float> result (ir.getNumChannels(), newLength);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        auto* source = ir.getWritePointer (ch);

        if (factor < 1.0)
        {
            // Shrinking reads the source faster, so band-limit it first.
            Biquad lowPass[2];
            for (auto& filter : lowPass)
            {
                filter.setLowPass (sampleRate, sampleRate * 0.45 * factor);
                for (int n = 0; n < oldLength; ++n)
                    source[n] = filter.process (0, source[n]);
            }
        }

        auto* dest = result.getWritePointer (ch);
        const double step = 1.0 / factor;

        for (int n = 0; n < newLength; ++n)
        {
            const double position = (double) n * step;
            const int index = (int) position;
            const float frac = (float) (position - (double) index);
            const float a = index < oldLength ? source[index] : 0.0f;
            const float b = index + 1 < oldLength ? source[index + 1] : 0.0f;
            dest[n] = a + frac * (b - a);
        }
    }

    ir = std::move (result);
}

void applyDecay (juce::AudioBuffer<float>& ir, double sampleRate, double decaySeconds)
{
    decaySeconds = juce::jmax (0.02, decaySeconds);
    const int length = ir.getNumSamples();
    const int keep = juce::jlimit (juce::jmin (64, length), length, (int) (decaySeconds * 1.2 * sampleRate));
    const double k = -6.9078 / (decaySeconds * sampleRate);

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        auto* data = ir.getWritePointer (ch);
        for (int n = 0; n < keep; ++n)
            data[n] *= (float) std::exp (k * (double) n);
    }

    if (keep < length)
        ir.setSize (ir.getNumChannels(), keep, true);
}

void applyAttack (juce::AudioBuffer<float>& ir, double sampleRate, double attackMs)
{
    const int attack = juce::jmin (ir.getNumSamples(), (int) (attackMs * 0.001 * sampleRate));

    for (int ch = 0; ch < ir.getNumChannels(); ++ch)
    {
        auto* data = ir.getWritePointer (ch);
        for (int n = 0; n < attack; ++n)
        {
            const float gain = (float) n / (float) attack;
            data[n] *= gain * gain;
        }
    }
}
} // namespace hgr::ir
