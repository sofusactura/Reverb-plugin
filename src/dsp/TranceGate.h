#pragma once

#include "Utils.h"

namespace hgr
{
/** Tempo-synced step gate. Follows the host timeline while it plays and free-runs at the last known tempo otherwise. */
class TranceGate
{
public:
    static constexpr int maxSteps = 16;

    struct Params
    {
        bool enabled = false;
        double stepQuarters = 0.25;   // length of one step in quarter notes
        int numSteps = 16;
        uint32_t pattern = 0xffff;    // bit i = step i is open
        float depth = 1.0f;           // 0..1, how far closed steps are attenuated
        float length = 0.5f;          // 0..1, open fraction of a step
        float attackMs = 2.0f;
        float releaseMs = 30.0f;
    };

    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        reset();
    }

    void reset() noexcept
    {
        position = 0.0;
        envelope = 1.0f;
    }

    /** ppq is the host position (in quarter notes) of the first sample; it is only used when hostSynced is true. */
    void process (float* left, float* right, int numSamples, const Params& p,
                  double bpm, bool hostSynced, double ppq) noexcept
    {
        const double stepQuarters = std::max (1.0e-4, p.stepQuarters);
        const double increment = std::clamp (bpm, 20.0, 999.0) / (60.0 * sampleRate * stepQuarters);

        if (hostSynced)
            position = ppq / stepQuarters;

        const int numSteps = std::clamp (p.numSteps, 1, maxSteps);
        const float floorGain = 1.0f - std::clamp (p.depth, 0.0f, 1.0f);
        const float attack = coefficient (p.attackMs);
        const float release = coefficient (p.releaseMs);

        for (int i = 0; i < numSamples; ++i)
        {
            float target = 1.0f;

            if (p.enabled)
            {
                const double whole = std::floor (position);
                int step = (int) std::fmod (whole, (double) numSteps);
                if (step < 0)
                    step += numSteps;

                currentStep = step;
                const bool open = ((p.pattern >> step) & 1u) != 0 && (position - whole) < (double) p.length;
                target = open ? 1.0f : floorGain;
            }

            envelope += (target > envelope ? attack : release) * (target - envelope);
            left[i] *= envelope;
            right[i] *= envelope;
            position += increment;
        }

        // Keep the free-running position bounded.
        if (position > 1.0e9)
            position = std::fmod (position, (double) numSteps);
    }

    /** The step the gate was on at the end of the last block (for the interface). */
    int getCurrentStep() const noexcept { return currentStep; }

private:
    float coefficient (float ms) const noexcept
    {
        return 1.0f - std::exp (-1.0f / (std::max (0.05f, ms) * 0.001f * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    double position = 0.0;
    float envelope = 1.0f;
    int currentStep = 0;
};
} // namespace hgr
