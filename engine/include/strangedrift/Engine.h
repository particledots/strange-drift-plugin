// The Strange Drift engine: three slow chaos circuits (ripple, tide, glacier; see Chaos.h) and a blend of them, sent as
// smooth 14-bit CC (see Outputs.h and CcStream.h).
//
// Plain C++17. Nothing here knows about JUCE, Logic or MIDI devices; the plugin hands in the control values and the
// incoming mod wheel, and gets back CC messages placed at exact sample positions.
//
// Two clocks, both counted from the start in samples, so nothing depends on how Logic slices the audio into blocks:
//   - the circuits move in fixed steps of 1 ms of real time (kStepsPerSecond), the same at every sample rate, so the
//     chaos takes exactly the same path at 44.1, 48 and 96 kHz;
//   - the CC values are sent at ticks of 32 samples, as in Ogdoad.

#pragma once

#include "strangedrift/CcStream.h"
#include "strangedrift/Chaos.h"
#include "strangedrift/Outputs.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

namespace strangedrift
{
    // The Speed knob (0..1) to a speed factor: 1/8 x fully anticlockwise, 1x at the centre, 512x fully clockwise,
    // even steps in octaves on each side of the centre.
    inline double speedFactor (double knob)
    {
        if (knob < 0.0) knob = 0.0;
        if (knob > 1.0) knob = 1.0;
        return knob < 0.5 ? std::exp2 (-3.0 * (0.5 - knob) * 2.0) : std::exp2 (9.0 * (knob - 0.5) * 2.0);
    }

    struct Settings
    {
        double speed = 0.5;                         // the Speed knob, 0..1 (0.5 = 1x)
        bool   hold  = false;                       // freeze all three
        double lean[kNumControlled]  { 0.0, 0.0 };  // -1..+1, ripple and tide
        double nudge[kNumControlled] { 0.0, 0.0 };  // -1..+1
        int    nudgeFrom[kNumControlled] { 0, 0 };  // NudgeFrom
    };

    struct InputEvent
    {
        int    offset = 0;                          // sample position inside the block
        double modWheel = 0.0;                      // 0..1
    };

    class Engine
    {
    public:
        static constexpr int kTickSamples = 32;
        static constexpr int kStepsPerSecond = 1000;
        static constexpr int kTracePoints = 400;            // the Orbits view: the last 400 points of each circuit
        static constexpr double kTraceUnits = 0.025;        // one trace point every 0.025 circuit units (10 units shown)

        Engine();

        // Sets the sample rate. Keeps the chaos where it is (a new sample rate does not restart it).
        void prepare (double sampleRate);

        // A fresh start: each circuit at a new place on its butterfly, chosen from the seed.
        void seed (uint64_t seedValue);

        void setSettings (const Settings& s) { settings = s; }
        const Settings& getSettings() const  { return settings; }

        // Runs one block. Input events must be sorted by offset. CC messages go to the sink with offsets inside the block.
        void render (int numSamples, const InputEvent* events, int numEvents, CcSink& sink);

        // Values for the panel and the tests (read after render).
        double output (int i) const          { return outputs[(std::size_t) i]; }    // -1..+1
        const Circuit& circuit (int c) const { return circuits[(std::size_t) c]; }
        double push (int c) const            { return pushes[(std::size_t) c]; }
        double unitsDone (int c) const       { return units[(std::size_t) c]; }     // circuit time travelled so far
        uint64_t stepsDone() const           { return steps; }
        uint64_t samplesDone() const         { return samples; }
        int rescues() const                  { return rescueCount; }               // times a circuit had to be restarted (never expected)
        bool isHeld() const                  { return settings.hold; }

        // The Orbits view: point k (0 = oldest) of circuit c, as (X, Y) in -1..+1.
        float traceX (int c, int k) const;
        float traceY (int c, int k) const;
        int traceCount (int c) const         { return traceFill[(std::size_t) c]; }

        // Where the chaos is, as text (saved with the project), and back. loadState returns false and changes nothing
        // if the text is not a valid state.
        std::string saveState() const;
        bool loadState (const std::string& text);

        // Copies where the chaos is (the circuits, their random state and the Orbits traces) from another engine,
        // without allocating; the clocks and settings stay. Everything is sent again at the next tick.
        void takePositionFrom (const Engine& other);

        CcStream& ccStream()                 { return cc; }

        static double blendOf (double rippleY, double tideY, double glacierY)
        {
            // The three Z outputs (Y upside down) added: tide + glacier - ripple; halved, and kept inside -1..+1.
            return clampUnit (0.5 * ((-tideY) + (-glacierY) - (-rippleY)));
        }

    private:
        void doStep();
        void tick (int offset, CcSink& sink);
        void updateOutputs();
        void recordTrace (int c);
        double nudgeSource (int controlled, int which) const;
        uint64_t sampleOfStep (uint64_t k) const { return (k * rateInt) / (uint64_t) kStepsPerSecond; }

        Settings settings;
        double sampleRate = 48000.0;
        uint64_t rateInt = 48000;
        uint64_t samples = 0, steps = 0, ticks = 0;
        int samplesToTick = 0;
        double modWheel = 0.0;

        std::array<Circuit, kNumCircuits> circuits {};
        std::array<double, kNumCircuits> units {}, pushes {}, traceAcc {};
        std::array<uint32_t, kNumCircuits> rng {};
        std::array<double, kNumOutputs> outputs {};
        int rescueCount = 0;

        std::array<std::array<float, kTracePoints * 2>, kNumCircuits> trace {};
        std::array<int, kNumCircuits> traceHead {}, traceFill {};

        CcStream cc;
    };
}
