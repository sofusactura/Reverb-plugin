#pragma once

#include "Utils.h"

namespace hgr
{
/** Algorithmic reverb: input diffusion into an 8-line modulated feedback delay network.
    Output is 100% wet. All memory is allocated in prepare(). */
class AlgoReverb
{
public:
    enum Mode { darkHall = 0, plate, shimmer, tides, echoes, numModes };

    struct Params
    {
        int mode = darkHall;
        float decaySeconds = 3.0f;
        float size = 0.5f;        // 0..1
        float dampingHz = 8000.0f;
        float diffusion = 0.8f;   // 0..1
        float modulation = 0.3f;  // 0..1
        float shimmer = 0.4f;     // 0..1, only used by the shimmer mode
        bool freeze = false;
    };

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        const float msToSamples = (float) (sampleRate * 0.001);

        for (int i = 0; i < numLines; ++i)
            lines[i].prepare ((int) (maxLineMs * msToSamples) + 64);

        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < numDiffusers; ++i)
            {
                diffusers[ch][i].line.prepare ((int) (diffuserMs[ch][i] * msToSamples) + 8);
                diffusers[ch][i].delay = std::max (1, (int) (diffuserMs[ch][i] * msToSamples));
            }

        for (auto& s : shifters)
            s.prepare (sampleRate);

        sizeSmoothing = 1.0f - std::exp (-1.0f / (0.12f * (float) sampleRate));
        reset();
    }

    void reset() noexcept
    {
        for (auto& l : lines)
            l.reset();

        for (auto& ch : diffusers)
            for (auto& d : ch)
                d.line.reset();

        for (auto& s : shifters)
            s.reset();

        std::fill (std::begin (dampState), std::end (dampState), 0.0f);
        lfoPhase = 0.0;
        sweepPhase = 0.0;
        controlCounter = 0;
        scaleSmoothed = -1.0f;
    }

    void setParams (const Params& newParams) noexcept { params = newParams; }

    void process (float* left, float* right, int numSamples) noexcept
    {
        const auto& cfg = modeConfigs[std::clamp (params.mode, 0, (int) numModes - 1)];
        const bool frozen = params.freeze;
        const float msToSamples = (float) (sampleRate * 0.001);

        const float scaleTarget = cfg.lengthScale * (0.4f + 1.4f * std::clamp (params.size, 0.0f, 1.0f));
        if (scaleSmoothed < 0.0f)
            scaleSmoothed = scaleTarget;

        const float diffusionGain = 0.75f * std::clamp (params.diffusion * cfg.diffusionScale, 0.0f, 1.0f);
        const float modDepth = frozen ? 0.0f
                                      : cfg.modDepthMs * msToSamples * std::clamp (params.modulation, 0.0f, 1.0f);
        const double lfoInc = cfg.modRateHz / sampleRate;
        const double sweepInc = 0.07 / sampleRate;
        const float shimmerMix = (params.mode == shimmer && ! frozen) ? 0.85f * std::clamp (params.shimmer, 0.0f, 1.0f) : 0.0f;

        for (int n = 0; n < numSamples; ++n)
        {
            if (! frozen)
                scaleSmoothed += sizeSmoothing * (scaleTarget - scaleSmoothed);

            if (controlCounter-- <= 0)
            {
                controlCounter = 31;
                updateControlRate (cfg, frozen);
            }

            float inL = frozen ? 0.0f : left[n];
            float inR = frozen ? 0.0f : right[n];

            for (int i = 0; i < numDiffusers; ++i)
            {
                inL = diffusers[0][i].process (inL, diffusionGain);
                inR = diffusers[1][i].process (inR, diffusionGain);
            }

            // Read the delay lines.
            float v[numLines];
            for (int i = 0; i < numLines; ++i)
            {
                float delay = baseLineMs[i] * msToSamples * scaleSmoothed;

                if (frozen)
                {
                    v[i] = lines[i].readInt (std::max (1, (int) delay));   // integer reads are lossless
                }
                else
                {
                    const float lfo = fastSin ((float) lfoPhase + (float) i * 0.125f);
                    delay = std::max (1.0f, delay + modDepth * (1.0f + lfo));
                    v[i] = lines[i].read (delay);
                    dampState[i] += dampCoeff * (v[i] - dampState[i]);
                    v[i] = dampState[i];
                }
            }

            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0)
                lfoPhase -= 1.0;

            sweepPhase += sweepInc;
            if (sweepPhase >= 1.0)
                sweepPhase -= 1.0;

            const float outL = v[0] - v[2] + v[4] - v[6];
            const float outR = v[1] - v[3] + v[5] - v[7];

            // Orthonormal Hadamard feedback matrix.
            hadamard8 (v);

            if (shimmerMix > 0.0f)
            {
                v[6] += shimmerMix * (shifters[0].process (v[6]) - v[6]);
                v[7] += shimmerMix * (shifters[1].process (v[7]) - v[7]);
            }
            else
            {
                shifters[0].push (v[6]);
                shifters[1].push (v[7]);
            }

            for (int i = 0; i < numLines; ++i)
            {
                const float input = (i & 1) == 0 ? inL : inR;
                const float x = input * inputSigns[i] + lineGain[i] * v[i];
                lines[i].write (std::clamp (x, -16.0f, 16.0f));   // guard against runaway feedback
            }

            left[n] = outL * outputGain;
            right[n] = outR * outputGain;
        }
    }

private:
    static constexpr int numLines = 8;
    static constexpr int numDiffusers = 4;
    static constexpr float maxLineMs = 420.0f;

    struct ModeConfig
    {
        float lengthScale, diffusionScale, modDepthMs, modRateHz, dampingScale;
        bool sweepDamping;
    };

    static constexpr ModeConfig modeConfigs[numModes] = {
        { 1.30f, 1.00f, 0.60f, 0.50f, 0.45f, false },   // dark hall
        { 0.55f, 1.00f, 0.25f, 1.10f, 1.00f, false },   // plate
        { 1.20f, 1.00f, 0.50f, 0.40f, 1.00f, false },   // shimmer
        { 1.10f, 0.90f, 2.50f, 0.12f, 0.80f, true  },   // tides
        { 2.30f, 0.25f, 0.30f, 0.30f, 1.00f, false },   // echoes
    };

    static constexpr float baseLineMs[numLines] = { 29.3f, 33.7f, 37.9f, 42.1f, 47.3f, 53.9f, 59.3f, 67.1f };
    static constexpr float inputSigns[numLines] = { 1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f };
    static constexpr float diffuserMs[2][numDiffusers] = { { 4.77f, 3.59f, 12.73f, 9.31f },
                                                           { 5.03f, 3.37f, 13.11f, 8.87f } };

    struct Allpass
    {
        DelayLine line;
        int delay = 1;

        float process (float x, float g) noexcept
        {
            const float delayed = line.readInt (delay);
            const float v = x - g * delayed;
            line.write (v);
            return delayed + g * v;
        }
    };

    /** Octave-up pitch shifter using two crossfaded delay taps. */
    struct OctaveShifter
    {
        void prepare (double sr)
        {
            window = (float) (sr * 0.06);
            line.prepare ((int) window + 8);
            phaseInc = 1.0f / window;
            reset();
        }

        void reset() noexcept
        {
            line.reset();
            phase = 0.0f;
        }

        void push (float x) noexcept { line.write (x); }

        float process (float x) noexcept
        {
            line.write (x);

            phase -= phaseInc;
            if (phase < 0.0f)
                phase += 1.0f;

            float phase2 = phase + 0.5f;
            if (phase2 >= 1.0f)
                phase2 -= 1.0f;

            const float s1 = fastSin (phase * 0.5f);   // sin(pi * phase)
            const float s2 = fastSin (phase2 * 0.5f);
            return s1 * s1 * line.read (1.0f + phase * window) + s2 * s2 * line.read (1.0f + phase2 * window);
        }

        DelayLine line;
        float window = 2048.0f, phase = 0.0f, phaseInc = 0.0f;
    };

    /** sin(2*pi*x) approximation for any x >= 0; accurate to roughly 0.1%. */
    static float fastSin (float x) noexcept
    {
        x -= (float) (int) x;                         // wrap to [0, 1)
        const float t = x < 0.5f ? x : x - 1.0f;      // [-0.5, 0.5)
        const float y = 16.0f * t * (0.5f - std::abs (t));
        return y * (0.775f + 0.225f * std::abs (y));
    }

    static void hadamard8 (float* v) noexcept
    {
        for (int step = 1; step < numLines; step <<= 1)
            for (int i = 0; i < numLines; i += step << 1)
                for (int j = i; j < i + step; ++j)
                {
                    const float a = v[j], b = v[j + step];
                    v[j] = a + b;
                    v[j + step] = a - b;
                }

        constexpr float norm = 0.35355339f;   // 1 / sqrt(8)
        for (int i = 0; i < numLines; ++i)
            v[i] *= norm;
    }

    void updateControlRate (const ModeConfig& cfg, bool frozen) noexcept
    {
        float damping = params.dampingHz * cfg.dampingScale;
        if (cfg.sweepDamping)
            damping *= std::pow (2.0f, 0.9f * fastSin ((float) sweepPhase));

        damping = std::clamp (damping, 200.0f, (float) (sampleRate * 0.45));
        dampCoeff = 1.0f - std::exp (-2.0f * (float) pi * damping / (float) sampleRate);

        const float decay = std::max (0.05f, params.decaySeconds);
        float gainSum = 0.0f;

        for (int i = 0; i < numLines; ++i)
        {
            const float lengthSeconds = baseLineMs[i] * 0.001f * scaleSmoothed;
            lineGain[i] = frozen ? 1.0f : std::pow (10.0f, -3.0f * lengthSeconds / decay);
            gainSum += lineGain[i];
        }

        // Partially compensate the level build-up of long decays.
        const float g = gainSum / (float) numLines;
        const float loss = std::clamp (1.0f - g * g, 0.02f, 1.0f);
        const float target = frozen ? outputGain : 0.5f * std::sqrt (std::sqrt (loss));
        outputGain += 0.2f * (target - outputGain);
    }

    double sampleRate = 44100.0;
    Params params;

    DelayLine lines[numLines];
    Allpass diffusers[2][numDiffusers];
    OctaveShifter shifters[2];

    float dampState[numLines] {};
    float lineGain[numLines] {};
    float dampCoeff = 1.0f;
    float outputGain = 0.3f;
    float sizeSmoothing = 0.001f;
    float scaleSmoothed = -1.0f;
    double lfoPhase = 0.0, sweepPhase = 0.0;
    int controlCounter = 0;
};
} // namespace hgr
