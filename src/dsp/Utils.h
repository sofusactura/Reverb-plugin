#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace hgr
{
constexpr double pi = 3.14159265358979323846;

/** Stereo biquad (RBJ cookbook), transposed direct form II. No allocation, safe to retune on the audio thread. */
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1[2] {}, z2[2] {};

    void reset() noexcept
    {
        z1[0] = z1[1] = z2[0] = z2[1] = 0.0f;
    }

    float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setLowPass (double sampleRate, double freq, double q = 0.7071) noexcept
    {
        const double w = omega (sampleRate, freq), c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set ((1.0 - c) * 0.5, 1.0 - c, (1.0 - c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

    void setHighPass (double sampleRate, double freq, double q = 0.7071) noexcept
    {
        const double w = omega (sampleRate, freq), c = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        set ((1.0 + c) * 0.5, -(1.0 + c), (1.0 + c) * 0.5, 1.0 + alpha, -2.0 * c, 1.0 - alpha);
    }

    void setLowShelf (double sampleRate, double freq, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = omega (sampleRate, freq), c = std::cos (w);
        const double beta = 2.0 * std::sqrt (A) * std::sin (w) / (2.0 * 0.7071);
        set (A * ((A + 1.0) - (A - 1.0) * c + beta),
             2.0 * A * ((A - 1.0) - (A + 1.0) * c),
             A * ((A + 1.0) - (A - 1.0) * c - beta),
             (A + 1.0) + (A - 1.0) * c + beta,
             -2.0 * ((A - 1.0) + (A + 1.0) * c),
             (A + 1.0) + (A - 1.0) * c - beta);
    }

    void setHighShelf (double sampleRate, double freq, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = omega (sampleRate, freq), c = std::cos (w);
        const double beta = 2.0 * std::sqrt (A) * std::sin (w) / (2.0 * 0.7071);
        set (A * ((A + 1.0) + (A - 1.0) * c + beta),
             -2.0 * A * ((A - 1.0) + (A + 1.0) * c),
             A * ((A + 1.0) + (A - 1.0) * c - beta),
             (A + 1.0) - (A - 1.0) * c + beta,
             2.0 * ((A - 1.0) - (A + 1.0) * c),
             (A + 1.0) - (A - 1.0) * c - beta);
    }

private:
    static double omega (double sampleRate, double freq) noexcept
    {
        return 2.0 * pi * std::clamp (freq, 5.0, sampleRate * 0.49) / sampleRate;
    }

    void set (double nb0, double nb1, double nb2, double na0, double na1, double na2) noexcept
    {
        const double inv = 1.0 / na0;
        b0 = (float) (nb0 * inv);
        b1 = (float) (nb1 * inv);
        b2 = (float) (nb2 * inv);
        a1 = (float) (na1 * inv);
        a2 = (float) (na2 * inv);
    }
};

/** Power-of-two circular delay line with linear-interpolated reads. */
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int size = 16;
        while (size < maxDelaySamples + 4)
            size <<= 1;

        buffer.assign ((size_t) size, 0.0f);
        mask = size - 1;
        writeIndex = 0;
    }

    void reset() noexcept
    {
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    }

    int getMaxDelay() const noexcept { return mask - 3; }

    void write (float x) noexcept
    {
        buffer[(size_t) writeIndex] = x;
        writeIndex = (writeIndex + 1) & mask;
    }

    /** Delay in samples, measured back from the next write position (so 1 = the last written sample). */
    float read (float delaySamples) const noexcept
    {
        const int whole = (int) delaySamples;
        const float frac = delaySamples - (float) whole;
        const float a = buffer[(size_t) ((writeIndex - whole) & mask)];
        const float b = buffer[(size_t) ((writeIndex - whole - 1) & mask)];
        return a + frac * (b - a);
    }

    float readInt (int delaySamples) const noexcept
    {
        return buffer[(size_t) ((writeIndex - delaySamples) & mask)];
    }

private:
    std::vector<float> buffer { std::vector<float> (16, 0.0f) };
    int mask = 15;
    int writeIndex = 0;
};

/** Small deterministic PRNG, so generated impulse responses are identical on every platform. */
struct XorShift
{
    explicit XorShift (uint32_t seed) : state (seed != 0 ? seed : 0x9e3779b9u) {}

    uint32_t next() noexcept
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    /** Uniform in [0, 1). */
    float uniform() noexcept { return (float) (next() >> 8) * (1.0f / 16777216.0f); }

    /** Uniform in [-1, 1). */
    float bipolar() noexcept { return uniform() * 2.0f - 1.0f; }

    uint32_t state;
};
} // namespace hgr
