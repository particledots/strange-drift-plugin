#include "Parameters.h"
#include "strangedrift/Engine.h"

namespace DriftParams
{
    juce::String speedFactorText (double f)
    {
        if (std::fabs (f - 1.0) < 0.005) return juce::String::fromUTF8 ("1\xc3\x97");
        if (f >= 10.0) return juce::String (juce::roundToInt (f)) + juce::String::fromUTF8 ("\xc3\x97");
        if (f >= 1.0)  return juce::String (f, 1) + juce::String::fromUTF8 ("\xc3\x97");
        if (f >= 0.25) return juce::String (f, 2) + juce::String::fromUTF8 ("\xc3\x97");
        return juce::String (f, 3) + juce::String::fromUTF8 ("\xc3\x97");
    }

    juce::String durationText (double s)
    {
        if (s < 1.0)    return juce::String (s, 2) + " s";
        if (s < 10.0)   return juce::String (s, 1) + " s";
        if (s < 100.0)  return juce::String (juce::roundToInt (s)) + " s";
        if (s < 600.0)  return juce::String (s / 60.0, 1) + " min";
        if (s < 3600.0) return juce::String (juce::roundToInt (s / 60.0)) + " min";
        return juce::String (s / 3600.0, 1) + " h";
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        const int v = 1;
        auto percent = [] (float x, int) { const int p = roundToInt (x * 100.0f); return (p > 0 ? "+" : "") + String (p) + "%"; };

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { speed, v }, "Speed", NormalisableRange<float> (0.0f, 1.0f), 0.5f,
            AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int)
                { return speedFactorText (strangedrift::speedFactor (x)); })));
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { hold, v }, "Hold", false));

        const char* circuit[2] = { "Ripple", "Tide" };
        for (int c = 0; c < 2; ++c)
        {
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { lean[c], v }, String (circuit[c]) + " lean",
                NormalisableRange<float> (-1.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
            layout.add (std::make_unique<AudioParameterFloat> (ParameterID { nudge[c], v }, String (circuit[c]) + " nudge",
                NormalisableRange<float> (-1.0f, 1.0f), 0.0f, AudioParameterFloatAttributes().withStringFromValueFunction (percent)));
            layout.add (std::make_unique<AudioParameterChoice> (ParameterID { nudgeFrom[c], v }, String (circuit[c]) + " nudge from",
                nudgeFromNames[c], 0));
        }
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { outChannel, v }, "MIDI out channel", 1, 16, 15));
        return layout;
    }
}
