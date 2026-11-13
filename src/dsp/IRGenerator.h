#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace hgr::ir
{
enum Type { room = 0, chamber, hall, cathedral, plate, reverse, numBuiltIn };

/** Synthesises a stereo impulse response. Deterministic: the same arguments always give the same samples.
    `size` (0.5..2) scales reflection times and darkens/brightens the space without changing the decay time. */
juce::AudioBuffer<float> generate (Type type, double sampleRate, double decaySeconds, double size);

/** Time-stretches an impulse response by `factor` (>1 = longer and lower). */
void resize (juce::AudioBuffer<float>& ir, double sampleRate, double factor);

/** Applies an exponential window that reaches -60 dB at `decaySeconds`, trimming the inaudible tail. */
void applyDecay (juce::AudioBuffer<float>& ir, double sampleRate, double decaySeconds);

/** Fades the start of the impulse response in over `attackMs`. */
void applyAttack (juce::AudioBuffer<float>& ir, double sampleRate, double attackMs);
} // namespace hgr::ir
