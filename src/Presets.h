#pragma once

#include <vector>

/** Factory presets. Each one lists only the parameters that differ from the defaults,
    in real units (the same numbers the interface shows). */
struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<std::pair<const char*, float>> values;
    unsigned gatePattern;   // bit i = step i + 1 open; 0 keeps the default pattern
};

const std::vector<FactoryPreset>& getFactoryPresets();
