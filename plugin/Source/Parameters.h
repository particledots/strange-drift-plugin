// The list of controls Logic can see and automate.
//
// The short ids below are what Logic stores in saved projects and automation. They were chosen once, before the
// first build, and are never renamed. 1 = ripple, 2 = tide (numbers, not names, so the circuits can be renamed later).

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace DriftParams
{
    inline constexpr const char* speed      = "speed";       // all three together, 1/8x .. 1x (centre) .. 512x
    inline constexpr const char* hold       = "hold";        // freeze all three
    inline constexpr const char* lean[2]      = { "lean1", "lean2" };            // -1..+1
    inline constexpr const char* nudge[2]     = { "nudge1", "nudge2" };          // -1..+1
    inline constexpr const char* nudgeFrom[2] = { "nudge1From", "nudge2From" };  // menu, see nudgeFromNames
    inline constexpr const char* outChannel = "outChannel";  // MIDI channel the CC goes out on, 1..16 (15 by default)

    // The Nudge from menus (order fixed: Off, Mod wheel, then the other two circuits' X and Y).
    inline const juce::StringArray nudgeFromNames[2] {
        { "Off", "Mod wheel", "Tide X", "Tide Y", "Glacier X", "Glacier Y" },       // ripple
        { "Off", "Mod wheel", "Ripple X", "Ripple Y", "Glacier X", "Glacier Y" },   // tide
    };

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // "1x", "4.0x", "0.25x", "512x" for a speed factor.
    juce::String speedFactorText (double factor);

    // "20 s", "1.3 min", "35 min", "4.7 h", "0.31 s" for a duration in seconds.
    juce::String durationText (double seconds);
}
