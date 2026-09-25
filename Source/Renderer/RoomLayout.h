// Spatial Audio - room layouts (speaker positions for a venue)
#pragma once

#include "JuceIncludes.h"
#include "../Core/Vec3.h"

namespace spatial
{

enum class SpeakerType
{
    regular = 0,      // full range, placed precisely by the panning
    sub,              // low end only, below the sub crossover
    transducer        // vibroacoustic transducer: low band, follows the sound among transducers
};

juce::String speakerTypeToString (SpeakerType t);            // "regular", "sub", "transducer"
juce::String speakerTypeDisplayName (SpeakerType t);         // "Regular", "Sub", "Transducer"
SpeakerType speakerTypeFromString (const juce::String& s);

struct Speaker
{
    juce::String name;
    SpeakerType type = SpeakerType::regular;
    Vec3 position;          // metres (x right, y front, z height above floor)
    int output = 1;         // interface output number, starting at 1
    float trimDb = 0.0f;    // level adjustment for this speaker
};

struct RoomLayout
{
    juce::String name = "Untitled room";
    float width = 6.0f;     // metres, left to right
    float depth = 6.0f;     // metres, back to front
    float height = 3.0f;    // metres, floor to ceiling
    std::vector<Speaker> speakers;

    juce::String toJson() const;
    static bool fromJson (const juce::String& json, RoomLayout& result, juce::String& errorMessage);

    int highestOutput() const;
    int nextFreeOutput() const;

    static std::vector<RoomLayout> builtInPresets();
};

} // namespace spatial
