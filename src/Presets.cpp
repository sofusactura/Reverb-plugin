#include "Presets.h"

// Choice parameters use their index:
//   routing   0 Serial, 1 Parallel, 2 Algorithm, 3 Convolution
//   irType    0 Room, 1 Chamber, 2 Hall, 3 Cathedral, 4 Plate, 5 Reverse
//   algoType  0 Dark Hall, 1 Plate, 2 Shimmer, 3 Tides, 4 Echoes
//   gateRate  0 1/4, 1 1/8, 2 1/8T, 3 1/16, 4 1/16T, 5 1/32
//   vintage   0 Off, 1 Subtle, 2 Old, 3 Older, 4 Extreme
//   preDelayDiv  0 1/64 ... 3 1/16, 5 1/8, 7 1/8D, 8 1/4
const std::vector<FactoryPreset>& getFactoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Init", "Default", {}, 0 },

        { "Big Hall", "Hall",
          { { "routing", 0 }, { "irType", 2 }, { "irDecay", 3.2f }, { "algoType", 0 }, { "algoDecay", 5.0f }, { "algoSize", 70 },
            { "blend", 55 }, { "preDelay", 22 }, { "lowCut", 90 }, { "mix", 30 } }, 0 },
        { "Small Room", "Room",
          { { "routing", 3 }, { "irType", 0 }, { "irDecay", 0.6f }, { "irSize", 85 }, { "preDelay", 2 }, { "lowCut", 120 }, { "mix", 24 } }, 0 },
        { "Vocal Plate", "Plate",
          { { "routing", 1 }, { "irType", 4 }, { "irDecay", 1.6f }, { "algoType", 1 }, { "algoDecay", 1.8f }, { "algoSize", 40 },
            { "algoDamping", 11000 }, { "blend", 60 }, { "preDelay", 30 }, { "lowCut", 180 }, { "highShelfGain", 2 }, { "mix", 22 } }, 0 },
        { "Drum Chamber", "Room",
          { { "routing", 1 }, { "irType", 1 }, { "irDecay", 1.1f }, { "algoType", 1 }, { "algoDecay", 0.9f }, { "algoSize", 25 },
            { "blend", 35 }, { "preDelay", 8 }, { "lowCut", 150 }, { "bassMono", 1 }, { "mix", 20 } }, 0 },

        { "Shimmer Heaven", "Ambient",
          { { "routing", 2 }, { "algoType", 2 }, { "algoDecay", 12.0f }, { "algoSize", 80 }, { "algoShimmer", 70 }, { "algoMod", 40 },
            { "algoDamping", 9000 }, { "preDelay", 40 }, { "lowCut", 140 }, { "width", 140 }, { "mix", 50 } }, 0 },
        { "Frozen Lake", "Ambient",
          { { "routing", 0 }, { "irType", 3 }, { "irDecay", 6.0f }, { "irAttack", 120 }, { "algoType", 3 }, { "algoDecay", 20.0f },
            { "algoSize", 90 }, { "algoMod", 70 }, { "blend", 70 }, { "width", 160 }, { "highCut", 8000 }, { "mix", 60 } }, 0 },
        { "Slow Bloom", "Ambient",
          { { "routing", 0 }, { "irType", 2 }, { "irDecay", 4.0f }, { "irAttack", 350 }, { "algoType", 0 }, { "algoDecay", 9.0f },
            { "algoDiffusion", 100 }, { "blend", 50 }, { "lowCut", 110 }, { "mix", 55 } }, 0 },

        { "Cold Cathedral", "Gated",
          { { "routing", 0 }, { "irType", 3 }, { "irDecay", 4.5f }, { "algoType", 0 }, { "algoDecay", 6.0f }, { "blend", 45 },
            { "lowCut", 130 }, { "mix", 45 }, { "gateOn", 1 }, { "gateRate", 3 }, { "gateLength", 60 }, { "gateRelease", 30 } },
          0xB5DDu /* x.xx x.xx x.x. xx.x */ },
        { "Trance Pump", "Gated",
          { { "routing", 2 }, { "algoType", 1 }, { "algoDecay", 3.0f }, { "algoSize", 55 }, { "lowCut", 160 }, { "mix", 50 },
            { "gateOn", 1 }, { "gateRate", 3 }, { "gateLength", 100 }, { "gateAttack", 8 }, { "gateRelease", 70 } },
          0xCCCCu /* ..xx ..xx ..xx ..xx : opens on the off-beat */ },
        { "Gallop Gate", "Gated",
          { { "routing", 2 }, { "algoType", 2 }, { "algoDecay", 7.0f }, { "algoShimmer", 35 }, { "lowCut", 150 }, { "mix", 48 },
            { "gateOn", 1 }, { "gateRate", 3 }, { "gateLength", 70 }, { "gateRelease", 20 } },
          0xDDDDu /* x.xx x.xx x.xx x.xx */ },
        { "Triplet Stutter", "Gated",
          { { "routing", 1 }, { "irType", 4 }, { "irDecay", 2.5f }, { "algoType", 3 }, { "algoDecay", 5.0f }, { "mix", 45 },
            { "gateOn", 1 }, { "gateRate", 4 }, { "gateSteps", 12 }, { "gateLength", 50 }, { "gateRelease", 12 } },
          0x0B6Du /* x.xx.xx.xx.x over 12 triplet steps */ },
        { "Soft Tremor", "Gated",
          { { "routing", 0 }, { "irType", 2 }, { "irDecay", 3.0f }, { "algoType", 0 }, { "algoDecay", 4.0f }, { "mix", 40 },
            { "gateOn", 1 }, { "gateRate", 1 }, { "gateSteps", 8 }, { "gateDepth", 55 }, { "gateLength", 50 }, { "gateAttack", 30 }, { "gateRelease", 180 } },
          0x55u /* x.x.x.x. */ },

        { "Dub Echoes", "FX",
          { { "routing", 2 }, { "algoType", 4 }, { "algoDecay", 5.0f }, { "algoSize", 65 }, { "algoDamping", 5000 }, { "preDelaySync", 1 },
            { "preDelayDiv", 7 }, { "preDelayFeedback", 55 }, { "vintage", 2 }, { "lowCut", 200 }, { "mix", 40 } }, 0 },
        { "Reverse Swell", "FX",
          { { "routing", 3 }, { "irType", 5 }, { "irDecay", 3.0f }, { "lowCut", 120 }, { "width", 130 }, { "mix", 50 } }, 0 },
        { "Lo-Fi Chamber", "FX",
          { { "routing", 3 }, { "irType", 1 }, { "irDecay", 1.8f }, { "vintage", 3 }, { "bassMono", 1 }, { "highCut", 5000 },
            { "lowCut", 200 }, { "width", 80 }, { "mix", 35 } }, 0 },
    };

    return presets;
}
